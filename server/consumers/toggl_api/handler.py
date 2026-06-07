"""consumer-toggl-api Lambda.

Trigger: EventBridge rule matching `detail-type` startswith
`device.session.`.

Maintains a 1:1 mapping between device "work" sessions and Toggl time
entries. Adopts an already-running entry on start to keep replays/restarts
idempotent.
"""

from __future__ import annotations
import json
import logging
import os

import boto3

from shared import events as ev
from shared import secrets as sec
from shared import state_store
from shared import toggl_client

log = logging.getLogger()
log.setLevel(logging.INFO)

CREDENTIALS_SECRET_ARN = os.environ["CREDENTIALS_SECRET_ARN"]
REGION = os.environ.get("AWS_REGION", "eu-central-1")

# IoT data-plane client for the desired.task_name push. Lazily instantiated
# so unit tests don't pay the boto cost when they monkey-patch it out.
_iot_data = boto3.client("iot-data", region_name=REGION)


def _config() -> tuple[str, int, int | None, str | None]:
    cfg = sec.get_secret(CREDENTIALS_SECRET_ARN).get("toggl") or {}
    return (
        cfg["api_token"],
        int(cfg["workspace_id"]),
        cfg.get("project_id") and int(cfg["project_id"]) or None,
        cfg.get("default_description"),
    )


# ---------------------------------------------------------------------------
# Per-event actions. Each takes (thing_name, detail dict) and returns nothing.
# State-store reads/writes are encapsulated here so the dispatch table below
# stays a clean event_type -> function map.
# ---------------------------------------------------------------------------

def _on_work_started(thing_name: str, detail: dict) -> None:
    """Start (or adopt) a Toggl entry to match the device's work session.

    Project / description resolution order:
      1. Stored task_context (last seen via Toggl webhook): replay user's
         most recent project + description so device-initiated sessions
         continue what they were last doing.
      2. Defaults from the m5pomodoro/toggl/api secret (project_id,
         default_description).

    After we have the entry, we ALSO write desired.task_name (and
    project_color) straight to the shadow. Without this the device LCD
    would wait for the Toggl webhook to round-trip back through
    source-toggl-webhook → consumer-task-context, which takes 5-10s and
    leaves the device showing "Focus Session" placeholder in the meantime.
    """
    api_token, ws_id, default_project_id, default_desc = _config()

    # Loop guard: if a Toggl entry is already running (probably because the
    # user started a PC-side timer and the device followed via the
    # device-shadow consumer), adopt it instead of double-starting.
    existing = toggl_client.current_entry(api_token)
    if existing:
        entry_id = int(existing["id"])
        log.info("Adopting already-running Toggl entry %d for %s",
                 entry_id, thing_name)
        state_store.set_running_entry(thing_name, entry_id)
        _push_task_name_for_project(
            thing_name, api_token, ws_id, existing.get("project_id"),
        )
        return

    # Pull stored context — only Toggl entries have project_id we trust here,
    # so we only honor it when the last context came from Toggl. Future
    # providers can plug into the same row but consumer-toggl-api would
    # ignore their provider_ref.
    project_id = default_project_id
    description = default_desc
    ctx = state_store.get_task_context(thing_name)
    if ctx and ctx.get("provider") == "toggl":
        ref = ctx.get("provider_ref") or {}
        if ref.get("toggl_project_id"):
            project_id = int(ref["toggl_project_id"])
        if ref.get("description"):
            description = ref["description"]
        log.info("Using stored Toggl context: project_id=%s description=%r",
                 project_id, description)
    else:
        log.info("No stored Toggl context; falling back to defaults")

    entry = toggl_client.start_entry(
        api_token,
        workspace_id=ws_id,
        project_id=project_id,
        description=description,
    )
    entry_id = int(entry["id"])
    state_store.set_running_entry(thing_name, entry_id)
    log.info("Started Toggl entry %d for %s", entry_id, thing_name)
    _push_task_name_for_project(thing_name, api_token, ws_id, project_id)


def _push_task_name_for_project(thing_name: str, api_token: str,
                                workspace_id: int,
                                project_id: int | None) -> None:
    """Resolve project_id → name+color and write them onto the shadow's
    desired state. Soft-fails — a network blip or deleted project must
    never break the work-start flow."""
    if not project_id:
        return
    try:
        project = toggl_client.get_project(
            api_token, workspace_id=workspace_id,
            project_id=int(project_id),
        )
    except Exception as e:                          # noqa: BLE001
        log.warning("get_project(%s) failed: %s", project_id, e)
        return
    if not project:
        return

    desired: dict = {}
    name = (project.get("name") or "").strip()
    if name:
        desired["task_name"] = name
    color = project.get("color")
    if color:
        desired["project_color"] = color
    if not desired:
        return

    body = json.dumps({"state": {"desired": desired}}).encode("utf-8")
    try:
        _iot_data.update_thing_shadow(thingName=thing_name, payload=body)
        log.info("Pushed desired %s -> shadow for %s",
                 list(desired.keys()), thing_name)
    except Exception as e:                          # noqa: BLE001
        log.warning("update_thing_shadow failed for %s: %s", thing_name, e)


def _on_work_paused_or_completed(thing_name: str, detail: dict) -> None:
    """Stop the running Toggl entry, if any."""
    api_token, ws_id, _, _ = _config()
    entry_id = state_store.get_running_entry(thing_name)
    if not entry_id:
        # We never saw a start (or the entry was already stopped + cleared).
        # Belt-and-braces: ask Toggl what's running and stop that.
        cur = toggl_client.current_entry(api_token)
        if cur:
            entry_id = int(cur["id"])
        else:
            log.info("No running Toggl entry to stop for %s", thing_name)
            return

    toggl_client.stop_entry(api_token, workspace_id=ws_id, entry_id=entry_id)
    state_store.clear_running_entry(thing_name)
    log.info("Stopped Toggl entry %d for %s", entry_id, thing_name)


# Dispatch table — adding a new device.session.* mapping is one line.
HANDLERS = {
    ev.DEVICE_WORK_STARTED:    _on_work_started,
    ev.DEVICE_WORK_RESUMED:    _on_work_started,
    ev.DEVICE_WORK_PAUSED:     _on_work_paused_or_completed,
    ev.DEVICE_WORK_COMPLETED:  _on_work_paused_or_completed,
    ev.DEVICE_BREAK_STARTED:   _on_work_paused_or_completed,
    # break_completed and cycle_completed are no-ops here; they don't change
    # the Toggl running state. (The break completion may transition into the
    # next work session, which fires its own work_started event.)
}


def handler(event: dict, context) -> dict:
    detail_type = event.get("detail-type") or event.get("DetailType")
    detail = event.get("detail") or {}
    thing_name = detail.get("thing_name")

    log.info("toggl_api detail-type=%s thing=%s detail=%s",
             detail_type, thing_name, json.dumps(detail)[:200])

    if not detail_type or not thing_name:
        log.warning("Missing detail-type or thing_name; ignoring")
        return {"ok": False, "reason": "malformed"}

    action = HANDLERS.get(detail_type)
    if not action:
        log.info("No action mapped for %s; skipping", detail_type)
        return {"ok": True, "skipped": detail_type}

    action(thing_name, detail)
    return {"ok": True, "handled": detail_type}
