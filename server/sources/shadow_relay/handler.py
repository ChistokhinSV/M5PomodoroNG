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

from shared import shadow_parser

log = logging.getLogger()
log.setLevel(logging.INFO)

_events = boto3.client(
    "events", region_name=os.environ.get("AWS_REGION", "eu-central-1")
)


def handler(event: dict, context) -> dict:
    log.info("shadow_relay event: %s", json.dumps(event)[:500])

    thing_name = event.get("thing_name")
    if not thing_name:
        log.warning("No thing_name on event; dropping")
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
        log.info("shadow_relay produced 0 events for thing=%s", thing_name)
        return {"emitted": 0}

    # EventBridge accepts up to 10 entries per PutEvents call. We only ever
    # emit a handful per shadow doc, so a single batch is enough.
    resp = _events.put_events(Entries=entries)
    failed = resp.get("FailedEntryCount", 0)
    if failed:
        log.error("put_events: %d/%d failed: %s",
                  failed, len(entries), resp.get("Entries"))

    log.info(
        "shadow_relay emitted %d events for thing=%s (%s)",
        len(entries) - failed, thing_name,
        ", ".join(e["DetailType"] for e in entries),
    )
    return {"emitted": len(entries) - failed, "failed": failed}
