"""consumer-task-context Lambda.

Trigger: EventBridge rule matching `detail-type` startswith `external.` and
ending in `.timer.started`. Provider-agnostic — every timer-start webhook
across every supported tool routes here.

Two side-effects per matched event:

  1. DynamoDB: write the task_context row keyed by thing_name. Downstream
     consumers (e.g. consumer-toggl-api) read it when starting a session
     on the device so they can replay the user's last project + description
     instead of falling back to global defaults.

  2. AWS IoT Device Shadow: publish desired.task_name. The firmware's
     ShadowPublisher delta handler receives it and pushes it into
     ScreenManager::setMainScreenTaskName so the LCD shows the project
     name instead of "Focus Session".

The "task_name" we display is derived from whichever signal looks most
useful: explicit project_name first, then description, then a fallback.
"""

from __future__ import annotations
import json
import logging
import os

import boto3

from shared import state_store

log = logging.getLogger()
log.setLevel(logging.INFO)

REGION = os.environ.get("AWS_REGION", "eu-central-1")
_iot_data = boto3.client("iot-data", region_name=REGION)


def _provider_from_detail_type(detail_type: str) -> str:
    """external.toggl.timer.started -> 'toggl', external.clockify.timer... ->
    'clockify'. Used to namespace the DDB row so the consumer-toggl-api
    knows whether to trust provider_ref's project_id."""
    parts = (detail_type or "").split(".")
    return parts[1] if len(parts) > 1 else "unknown"


def _choose_task_name(detail: dict) -> str:
    """Pick the most readable label from the event payload."""
    # Toggl fills project_name for entries that have a project assigned.
    name = detail.get("project_name")
    if name and str(name).strip():
        return str(name).strip()

    desc = detail.get("description")
    if desc and str(desc).strip():
        return str(desc).strip()

    # Last resort: a non-empty string so the LCD never shows "" (which would
    # look broken).
    return "Tracked session"


def _publish_task_name(thing_name: str, task_name: str) -> None:
    body = {"state": {"desired": {"task_name": task_name}}}
    _iot_data.update_thing_shadow(
        thingName=thing_name,
        payload=json.dumps(body).encode("utf-8"),
    )


def handler(event: dict, context) -> dict:
    detail_type = event.get("detail-type") or event.get("DetailType")
    detail = event.get("detail") or {}
    thing_name = detail.get("thing_name")

    log.info(
        "task_context detail-type=%s thing=%s detail=%s",
        detail_type, thing_name, json.dumps(detail)[:200],
    )

    if not thing_name:
        log.warning("Missing thing_name; ignoring")
        return {"ok": False, "reason": "no_thing_name"}

    provider = _provider_from_detail_type(detail_type)
    task_name = _choose_task_name(detail)

    # Stash provider-specific tokens opaquely. Toggl: project_id, etc.
    # Plus project_name and project_color so downstream consumers
    # (gcal_api in particular) can render the label and pick a calendar
    # color matching the Toggl project without a second webhook lookup.
    kept_keys = {"description", "tags", "project_name", "project_color"}
    provider_ref = {
        k: v for k, v in detail.items()
        if k.startswith(f"{provider}_") or k in kept_keys
    }

    state_store.set_task_context(
        thing_name,
        task_name=task_name,
        provider=provider,
        provider_ref=provider_ref,
    )
    log.info("Stored task_context thing=%s provider=%s task_name=%r",
             thing_name, provider, task_name)

    _publish_task_name(thing_name, task_name)
    log.info("Published desired.task_name=%r to thing=%s", task_name, thing_name)

    return {"ok": True, "task_name": task_name, "provider": provider}
