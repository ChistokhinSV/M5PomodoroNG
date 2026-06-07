# m5pomodoro server (AWS SAM)

Server-side companion app for the M5 Pomodoro v2 device. Subscribes to the
device's AWS IoT Device Shadow, mirrors work sessions into Toggl Track,
creates Google Calendar events for completed sessions, and accepts Toggl
Track webhooks so PC-side timer changes flow back to the device.

## Architecture

```
SOURCES (publish events to the bus)
  shadow-relay    (IoT Topic Rule -> Lambda)  -> device.session.*
  toggl-webhook   (API Gateway -> Lambda)     -> external.toggl.*

EVENT BUS
  EventBridge custom bus "m5pomodoro-events"

CONSUMERS (subscribe to filtered slices)
  toggl-api       (device.session.*)            -> Toggl v9 REST
  gcal-api        (device.session.{work,cycle}.completed) -> Calendar v3
  device-shadow   (external.toggl.*)            -> AWS IoT shadow desired

STATE
  DynamoDB m5pomodoro-state    (per-device runtime + idempotency markers)
  Secrets Manager              (one combined credentials secret)
```

EventBridge rules do the routing — adding a new consumer is a CloudFormation
diff, not a refactor. Add a new webhook source by copying
`sources/toggl_webhook/`, picking a new event namespace, and adding three
resources to `template.yaml` (API path + Lambda + secret reference).

See `../docs/` for the device-side shadow document schema; the Lambdas
consume `device_state`, `session_type`, `session_number`, `total_sessions`,
`duration_min`, `today`, `week`, `lifetime`, `last_event`, `last_event_at`,
and the firmware-generated `event_id` derived from those.

## Prerequisites

- AWS account with IoT Core in use (the device firmware is already wired
  to it; see the project's main README)
- [AWS SAM CLI](https://docs.aws.amazon.com/serverless-application-model/latest/developerguide/install-sam-cli.html)
- Python 3.11 (for local tests and `sam build`)
- A Toggl Track account
- A Google Cloud project (for the Calendar service account)

## One-time setup

### 1. Toggl Track

1. Profile → **API Token**: copy it.
2. Profile → look up your **Workspace ID** (the integer in the URL when you
   view a workspace).
3. (Optional) Pick a project to charge Pomodoros to and grab its
   **Project ID**.
4. Workspace → Integrations → **Webhooks** → Add Webhook:
   - URL: leave blank for now (you'll paste the SAM output here after
     deploy)
   - Events: tick `time_entry.created`, `time_entry.updated`,
     `time_entry.deleted`
   - **Save** — Toggl generates a signing secret. Copy it.

### 2. Google Calendar

1. Google Cloud Console → IAM & Admin → **Service Accounts** → Create
   service account (e.g. `m5pomodoro-bridge`).
2. **Keys → Add Key → JSON** — download. This is the file you'll put in
   Secrets Manager.
3. Google Calendar → open the target calendar → **Settings and sharing**
   → "Share with specific people" → add the service account's email
   address with permission **Make changes to events**.
4. Copy the calendar's **Calendar ID** (Settings → Integrate calendar).

### 3. Secrets Manager

Everything goes into one secret. The shape:

```json
{
  "gcal_service_account": { ...the entire GCP service-account JSON... },
  "toggl": {
    "api_token":             "YOUR_TOGGL_API_TOKEN",
    "workspace_id":          12345678,
    "project_id":            87654321,
    "default_description":   "Pomodoro work",
    "webhook_signing_secret":"WEBHOOK_SECRET_FROM_TOGGL"
  }
}
```

Build the JSON locally first (so the SA key stays on one line), then:

```bash
aws secretsmanager create-secret --name m5pomodoro/credentials \
  --description "All credentials for the m5pomodoro server" \
  --secret-string file://credentials.json
```

Why one secret instead of three? AWS bills $0.40 per secret per month — one
combined secret is $0.40/mo instead of $1.20/mo. The trade-off is that every
Lambda role can now read every key. On a single-user single-device setup
that's an acceptable IAM-scoping loss.

Note the ARN printed — it's the `CredentialsSecretArn` parameter for
`sam deploy`.

## Deploy

```bash
cd server
sam build
sam deploy --guided
```

You'll be asked for:

| Parameter             | Value                                                   |
|-----------------------|---------------------------------------------------------|
| `ThingName`           | `M5StackCore2`                                          |
| `CalendarId`          | Your calendar ID from setup step 2.4                    |
| `CredentialsSecretArn`| Output ARN of `m5pomodoro/credentials`                  |

Save the answers to `samconfig.toml` when prompted so subsequent
`sam deploy` runs are silent. `samconfig.toml` is gitignored — see
`samconfig.toml.example` for the shape to copy if you want to fill it
in by hand.

Once deploy finishes, look at the **Outputs** section for the
`TogglWebhookUrl` — copy that URL back into Toggl's webhook config (step
1.4 above).

## Verification

### Unit tests

```bash
python -m pip install -r requirements-dev.txt
python -m pytest tests/ -v
```

Tests are pure-Python and don't hit AWS — the boto3 clients and HTTP
clients are mocked in fixtures.

### MQTT smoke test (device → cloud)

In AWS Console → IoT Core → **MQTT test client**, publish to topic
`$aws/things/M5StackCore2/shadow/update`:

```json
{
  "state": {
    "reported": {
      "device_state": "active",
      "session_type": "work",
      "session_number": 1,
      "total_sessions": 4,
      "duration_min": 25,
      "today": 0,
      "week": 0,
      "last_event": "work_started",
      "last_event_at": 1717612345,
      "timestamp": 1717612345
    }
  }
}
```

Expect within ~2s:
- A new running entry in your Toggl workspace.
- CloudWatch logs for `ShadowRelayFunction` and `TogglApiFunction` showing
  the dispatched event.

Then publish a follow-up reported state with `device_state: "idle"` and
`last_event: "work_complete"` (bumping `last_event_at`):

- Toggl entry should stop.
- A new Calendar event should appear.

### End-to-end (real device)

Tap Start on the M5. Within 2-3s the Toggl timer should appear. Complete
a session (or wait it out / set a 1-min duration in settings for testing).
A Calendar event materializes.

### Bidirectional (PC → device)

With the device idle, click **Start** on a time entry in Toggl Track
(desktop or web). The webhook fires → device-shadow consumer publishes
`desired.command=start` → device picks it up via the delta and the timer
starts on the hardware.

## DLQ replay

Each consumer has its own SQS dead-letter queue. If e.g. the Toggl API is
down for several minutes, events accumulate there. After the issue is
resolved:

```bash
# Lambda Console -> consumer-toggl-api -> Configuration -> DLQ -> Redrive
# Or via CLI:
aws lambda start-message-move-task \
  --source-arn arn:aws:sqs:eu-central-1:ACCOUNT:m5pomodoro-toggl-api-dlq \
  --destination-arn arn:aws:lambda:eu-central-1:ACCOUNT:function:...
```

## Adding a new webhook source

1. `cp -r sources/toggl_webhook sources/<svc>_webhook`
2. Change the classifier in `handler.py` for your source's payload shape;
   emit `external.<svc>.*` event types (add them to `shared/events.py`).
3. In `template.yaml`:
   - Add a new `AWS::Serverless::Function` for the receiver Lambda.
   - Add a new `Events:` entry on the Api with path `/webhooks/<svc>`.
   - Add a new Secrets Manager ARN parameter for that webhook's secret.
4. Add a consumer rule with `detail-type: prefix: external.<svc>.` if you
   need that source to route to a consumer.

## Adding a new consumer

1. `cp -r consumers/toggl_api consumers/<name>`
2. Rewrite `handler.py` to act on whatever subset of events you care about.
3. In `template.yaml`:
   - Add a new `AWS::Serverless::Function`.
   - Add a new `AWS::Events::Rule` with the event pattern that matches
     what your consumer needs.
   - Add a DLQ + RetryPolicy if it's a non-trivial side effect.

## Layout

```
server/
├── README.md                       (this file)
├── template.yaml                   (SAM CloudFormation)
├── samconfig.toml
├── requirements-dev.txt
├── shared/
│   └── python/shared/              (Lambda-layer convention: python/<module>/)
│       ├── __init__.py
│       ├── events.py               (detail-type constants + dataclasses)
│       ├── secrets.py              (Secrets Manager with module cache)
│       ├── state_store.py          (DynamoDB ops)
│       └── shadow_parser.py        (diff prev/cur shadow -> events)
├── sources/
│   ├── shadow_relay/handler.py
│   └── toggl_webhook/handler.py
├── consumers/
│   ├── toggl_api/{handler.py,toggl_client.py}
│   ├── gcal_api/{handler.py,gcal_client.py}
│   └── device_shadow/handler.py
└── tests/
    ├── conftest.py
    ├── test_shadow_parser.py
    ├── test_toggl_webhook.py
    ├── test_toggl_api.py
    ├── test_gcal_api.py
    └── test_device_shadow.py
```
