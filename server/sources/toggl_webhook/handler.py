"""source-toggl-webhook Lambda.

Trigger: API Gateway POST /webhooks/toggl.

Toggl Track sends a JSON body with HMAC-SHA256 signature in the
`x-webhook-signature-256` header. We validate, classify into
`external.toggl.timer.{started,stopped}`, and put_events to the bus.

The matching device is read from env (`TARGET_THING_NAME`). Multi-device
support would mean attaching the thing-name to the Toggl webhook
configuration somehow (e.g. URL path param, custom header) — out of scope
for the first cut; one Toggl webhook -> one device.
"""

from __future__ import annotations
import hashlib
import hmac
import json
import logging
import os
import time

import base64

import boto3
import urllib.error
import urllib.request

from shared import events as ev
from shared import logctx
from shared import secrets as sec

log = logging.getLogger()
log.setLevel(logging.INFO)

# Env wiring: SAM template provides these at deploy time.
TARGET_THING_NAME       = os.environ["TARGET_THING_NAME"]
CREDENTIALS_SECRET_ARN  = os.environ["CREDENTIALS_SECRET_ARN"]
EVENT_BUS_NAME          = os.environ.get("EVENT_BUS_NAME", ev.DEFAULT_BUS)

# Module-level cache so warm invocations don't re-hit Toggl for the same
# project. Cold-start hits Toggl once per project; thereafter free.
# Value is (name, color_hex_or_none) so a single lookup covers both fields.
_project_info_cache: dict[int, tuple[str | None, str | None]] = {}

_events = boto3.client(
    "events", region_name=os.environ.get("AWS_REGION", "eu-central-1")
)


def _response(status: int, body: dict | str = "") -> dict:
    """API Gateway proxy response."""
    return {
        "statusCode": status,
        "headers": {"content-type": "application/json"},
        "body": json.dumps(body) if isinstance(body, dict) else body,
    }


def _toggl_section() -> dict:
    return sec.get_secret(CREDENTIALS_SECRET_ARN).get("toggl") or {}


def _verify_signature(raw_body: bytes, header_sig: str) -> bool:
    """Toggl signs with HMAC-SHA256 hex of the raw request body, keyed by
    the per-webhook signing secret. Header form is
    `sha256=<64-hex-chars>`."""
    if not header_sig:
        return False
    signing_secret = _toggl_section().get("webhook_signing_secret")
    if not signing_secret:
        log.warning("toggl.webhook_signing_secret missing from credentials")
        return False
    expected = hmac.new(
        signing_secret.encode("utf-8"), raw_body, hashlib.sha256
    ).hexdigest()
    # Strip optional "sha256=" prefix; constant-time compare.
    received = header_sig.removeprefix("sha256=").strip()
    return hmac.compare_digest(expected, received)


def _resolve_project_info(project_id: int,
                          workspace_id: int) -> tuple[str | None, str | None]:
    """Look up Toggl's project display name and color. Returns (None, None)
    if we don't have Toggl credentials configured or if the API call fails —
    callers fall back to other label sources (and skip the GCal color)."""
    if not project_id:
        return None, None
    if project_id in _project_info_cache:
        return _project_info_cache[project_id]

    api_token = _toggl_section().get("api_token")
    if not api_token:
        log.warning("Toggl API token missing from credentials; can't resolve project")
        return None, None

    auth = base64.b64encode(f"{api_token}:api_token".encode()).decode("ascii")
    url = (f"https://api.track.toggl.com/api/v9/workspaces/{workspace_id}"
           f"/projects/{project_id}")
    req = urllib.request.Request(url, headers={
        "Authorization": f"Basic {auth}",
        "User-Agent": "m5pomodoro-bridge",
    })
    try:
        with urllib.request.urlopen(req, timeout=4) as resp:
            data = json.load(resp)
    except (urllib.error.URLError, urllib.error.HTTPError, OSError, ValueError) as e:
        log.warning("Toggl project lookup failed for id=%s: %s", project_id, e)
        return None, None

    name  = data.get("name")
    color = data.get("color")  # "#RRGGBB" per Toggl v9
    log.info("Toggl project %s/%s resolved name=%r color=%r",
             workspace_id, project_id, name, color)
    if name:
        _project_info_cache[project_id] = (name, color)
    return name, color


def _classify(payload: dict) -> tuple[str | None, dict]:
    """Map a Toggl webhook payload to a detail-type + extracted fields.

    Toggl's documented webhook event names for time entries:
      "time_entry"  + action in {created, updated, deleted}
    Plus a `payload` sub-field with the time entry itself, which has
    `duration` (negative when running, positive when stopped).
    """
    metadata = payload.get("metadata") or {}
    # Toggl v9 puts {action, model} in metadata; older test payloads put model
    # directly on the envelope. Accept either rather than ANDing on event_user_id
    # (which the previous implementation did, dropping every payload that
    # happened to omit it).
    entity   = metadata.get("model") or payload.get("model")
    action   = metadata.get("action") or payload.get("action")
    entry    = payload.get("payload") or payload  # both shapes seen in the wild

    if entity != "time_entry":
        return None, {}

    duration = entry.get("duration")
    # Running entries have negative duration in Toggl v9 conventions.
    is_running = isinstance(duration, (int, float)) and duration < 0

    if action == "created" or (action == "updated" and is_running):
        detail_type = ev.EXTERNAL_TOGGL_STARTED
    elif action in ("updated", "deleted") and not is_running:
        detail_type = ev.EXTERNAL_TOGGL_STOPPED
    else:
        return None, {}

    return detail_type, {
        "toggl_entry_id":    int(entry.get("id", 0)),
        "toggl_project_id":  entry.get("project_id"),
        "toggl_workspace_id": entry.get("workspace_id"),
        "description":       entry.get("description") or None,
        "tags":              list(entry.get("tags") or []),
    }


def handler(event: dict, context) -> dict:
    logger = logctx.bind(
        log, aws_request_id=logctx.request_id(context),
        extra={"thing": TARGET_THING_NAME},
    )

    # --- 1. signature ------------------------------------------------------
    raw_body = (event.get("body") or "").encode("utf-8")
    if event.get("isBase64Encoded"):
        import base64
        raw_body = base64.b64decode(raw_body)

    headers = {k.lower(): v for k, v in (event.get("headers") or {}).items()}
    if not _verify_signature(raw_body, headers.get("x-webhook-signature-256", "")):
        logger.warning("Signature mismatch; rejecting")
        return _response(401, {"error": "bad signature"})

    # --- 2. Toggl "validation_code" ping ----------------------------------
    # On webhook creation Toggl POSTs a one-shot {"validation_code": "..."}
    # and expects us to echo it back. Handle that before normal classify.
    try:
        body = json.loads(raw_body or b"{}")
    except json.JSONDecodeError:
        return _response(400, {"error": "bad json"})

    if "validation_code" in body:
        logger.info("Returning validation_code for Toggl webhook handshake")
        return _response(200, {"validation_code": body["validation_code"]})

    # Surface the cause of every incoming event before classification. If
    # Toggl Desktop's autotracker or some other client is restarting the
    # entry the user just stopped, this is the layer that sees the truth.
    # `subscription_id` / `webhook_id` / `created_at` come straight from
    # Toggl's webhook envelope; the metadata block carries the action +
    # entity model + the user id that performed the action. Together they
    # answer "who did this and from which client?".
    metadata = (body.get("metadata") or {})
    entry_payload = body.get("payload") or {}
    logger.info(
        "toggl webhook ingress action=%s model=%s "
        "creator_id=%s event_user_id=%s entry_id=%s description=%r "
        "project_id=%s duration=%s start=%s stop=%s tags=%s "
        "client_name=%r request_type=%s subscription_id=%s "
        "event_id=%s headers_ua=%r",
        metadata.get("action") or body.get("action"),
        metadata.get("model") or body.get("model"),
        entry_payload.get("user_id") or entry_payload.get("uid"),
        metadata.get("event_user_id"),
        entry_payload.get("id"),
        entry_payload.get("description"),
        entry_payload.get("project_id"),
        entry_payload.get("duration"),
        entry_payload.get("start"),
        entry_payload.get("stop"),
        entry_payload.get("tags"),
        # Toggl Desktop sends X-Client-Name / Client-Name headers; surface them.
        headers.get("client-name") or headers.get("x-client-name")
            or metadata.get("origin"),
        metadata.get("request_type"),
        body.get("subscription_id") or body.get("webhook_id"),
        body.get("event_id"),
        headers.get("user-agent"),
    )

    # --- 3. classify + emit ------------------------------------------------
    detail_type, extracted = _classify(body)
    if not detail_type:
        logger.info("Webhook payload classified as no-op; ignoring "
                    "(action=%s entity=%s)",
                    metadata.get("action") or body.get("action"),
                    metadata.get("model") or body.get("model"))
        return _response(200, {"ignored": True})

    ts = int(time.time())
    # Resolve project name + color for "started" events so downstream
    # consumers can render the label on the device LCD and tint GCal events
    # to roughly match Toggl. Stopped events skip the lookup (we don't
    # change task_name on stop).
    project_name:  str | None = None
    project_color: str | None = None
    if (detail_type == ev.EXTERNAL_TOGGL_STARTED
            and extracted["toggl_project_id"]
            and extracted["toggl_workspace_id"]):
        project_name, project_color = _resolve_project_info(
            int(extracted["toggl_project_id"]),
            int(extracted["toggl_workspace_id"]),
        )

    detail = ev.ExternalTogglDetail(
        thing_name=TARGET_THING_NAME,
        timestamp=ts,
        toggl_entry_id=extracted["toggl_entry_id"],
        toggl_project_id=extracted["toggl_project_id"],
        project_name=project_name,
        project_color=project_color,
        description=extracted["description"],
        tags=extracted["tags"],
        event_id=ev.synth_event_id(TARGET_THING_NAME, ts, detail_type),
    )
    entry = ev.external_toggl_event(detail_type, detail, bus=EVENT_BUS_NAME)

    resp = _events.put_events(Entries=[entry])
    failed = resp.get("FailedEntryCount", 0)
    if failed:
        logger.error("put_events failed: %s", resp.get("Entries"))
        return _response(502, {"error": "bus enqueue failed"})

    logger.info("Emitted %s for entry=%d event_id=%s",
                detail_type, extracted["toggl_entry_id"], detail.event_id)
    return _response(200, {"emitted": detail_type})
