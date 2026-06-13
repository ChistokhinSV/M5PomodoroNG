"""source-shadow-relay Lambda.

Trigger: AWS IoT Topic Rule on `$aws/things/+/shadow/update/documents`.

Input event (delivered by IoT Rules with `SELECT *, topic(3) AS thing_name
FROM '...'`):
    {
      "thing_name": "M5StackCore2",
      "previous": { ... shadow snapshot ... },
      "current":  { ... shadow snapshot ... },
      "timestamp": 1717612345
    }

Output: zero-to-many entries on the EventBridge custom bus, namespaced
`device.session.*`. The parse-and-emit logic lives in
`shared/shadow_parser.py` so unit tests cover the diff cases without
touching boto3.
"""

from __future__ import annotations
import json
import logging
import os

import boto3

from shared import logctx
from shared import shadow_parser

log = logging.getLogger()
log.setLevel(logging.INFO)

_events = boto3.client(
    "events", region_name=os.environ.get("AWS_REGION", "eu-central-1")
)


def handler(event: dict, context) -> dict:
    thing_name = event.get("thing_name")
    # Shadow `version` from `current` is the same number the device prints in
    # its [Shadow] Delta v=N logs. Surface it on every log line emitted in
    # this invocation so cloud/device entries can be matched 1:1.
    current_doc = (event.get("current") or {})
    shadow_version = current_doc.get("version")

    logger = logctx.bind(
        log,
        aws_request_id=logctx.request_id(context),
        shadow_version=shadow_version,
        extra={"thing": thing_name},
    )
    logger.info("shadow_relay event=%s", json.dumps(event)[:500])

    if not thing_name:
        logger.warning("No thing_name on event; dropping")
        return {"emitted": 0}

    # IoT Rule passes the original shadow `documents` payload through under
    # whatever shape SELECT * produces. The fields previous/current/timestamp
    # sit at the top level alongside thing_name.
    document = {
        "previous":  event.get("previous"),
        "current":   event.get("current"),
        "timestamp": event.get("timestamp"),
    }

    entries = shadow_parser.parse(thing_name, document)
    if not entries:
        logger.info("shadow_relay produced 0 events")
        return {"emitted": 0}

    # EventBridge accepts up to 10 entries per PutEvents call. We only ever
    # emit a handful per shadow doc, so a single batch is enough.
    resp = _events.put_events(Entries=entries)
    failed = resp.get("FailedEntryCount", 0)
    if failed:
        logger.error("put_events: %d/%d failed: %s",
                     failed, len(entries), resp.get("Entries"))

    # Emit detail-types AND event_ids so downstream consumer logs can be
    # joined back to this invocation by event_id alone.
    summaries = []
    for e in entries:
        try:
            d = json.loads(e["Detail"])
            summaries.append(f"{e['DetailType']}#{d.get('event_id', '?')}")
        except Exception:                           # noqa: BLE001
            summaries.append(e["DetailType"])
    logger.info("shadow_relay emitted %d events: %s",
                len(entries) - failed, ", ".join(summaries))
    return {"emitted": len(entries) - failed, "failed": failed}
