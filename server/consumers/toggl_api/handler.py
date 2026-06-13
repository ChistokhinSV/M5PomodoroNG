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
from shared import logctx
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
        _lg().info("Adopting already-running Toggl entry %d for %s",
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
        _lg().info("Using stored Toggl context: project_id=%s description=%r",
                   project_id, description)
    else:
        _lg().info("No stored Toggl context; falling back to defaults")

    entry = toggl_client.start_entry(
        api_token,
        workspace_id=ws_id,
        project_id=project_id,
        description=description,
    )
    entry_id = int(entry["id"])
    state_store.set_running_entry(thing_name, entry_id)
    _lg().info("Started Toggl entry %d for %s", entry_id, thing_name)
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
        _lg().warning("get_project(%s) failed: %s", project_id, e)
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

    # Mirror each desired key into reported as null so AWS always sees a
    # diff and delivers the field — even when reported.task_name happens
    # to match desired (e.g. shadow carries a stale value from before a
    # reflash and the device's in-memory last_task_name_ reset to empty
    # at boot). Same pattern as device_shadow's reported.command nulling.
    reported_clear = {k: None for k in desired}
    body = json.dumps({
        "state": {"desired": desired, "reported": reported_clear}
    }).encode("utf-8")
    try:
        _iot_data.update_thing_shadow(thingName=thing_name, payload=body)
        _lg().info("Pushed desired %s -> shadow for %s",
                   list(desired.keys()), thing_name)
    except Exception as e:                          # noqa: BLE001
        _lg().warning("update_thing_shadow failed for %s: %s", thing_name, e)


def _on_work_resumed(thing_name: str, detail: dict) -> None:
    """Mirror the device's resume into Toggl.

    A device.session.work.resumed event means the firmware just transitioned
    PAUSED → ACTIVE. There are three sub-cases distinguished by
    state_change_source (firmware-stamped, defaulting to "device"):

      A. source=="device" (user pressed the unpause button / gyro / etc.):
         Toggl has no running entry (we stopped it on the matching pause).
         Start a fresh entry replaying the most recent project context —
         this is the "I'm back, get Toggl going again" case.

      B. source=="shadow_command" + Toggl already running: device-shadow
         pushed a "resume" verb in response to a Toggl webhook, which means
         the Toggl entry that fired the webhook is the one to adopt.

      C. source=="shadow_command" + Toggl NOT running: stale shadow delivery
         (e.g. WiFi drop / reconnect). Don't auto-create — that would loop
         with whatever already stopped Toggl. Just clear the DDB pointer.
    """
    api_token, ws_id, default_project_id, default_desc = _config()
    source = (detail.get("state_change_source") or "device").lower()
    existing = toggl_client.current_entry(api_token)

    if existing:
        entry_id = int(existing["id"])
        _lg().info("RESUMED source=%s: adopting Toggl entry %d for %s",
                   source, entry_id, thing_name)
        state_store.set_running_entry(thing_name, entry_id)
        _push_task_name_for_project(
            thing_name, api_token, ws_id, existing.get("project_id"),
        )
        return

    if source != "device":
        # Case C — stale shadow-driven resume + no entry to adopt. Don't
        # create one or we'll fight with whatever stopped Toggl.
        state_store.clear_running_entry(thing_name)
        _lg().info("RESUMED source=%s and no Toggl entry running; "
                   "not auto-starting (stale-delta guard). thing=%s",
                   source, thing_name)
        return

    # Case A — device-initiated resume + no entry. Replay the user's most
    # recent project + description, same way _on_work_started does cold-start.
    project_id = default_project_id
    description = default_desc
    ctx = state_store.get_task_context(thing_name)
    if ctx and ctx.get("provider") == "toggl":
        ref = ctx.get("provider_ref") or {}
        if ref.get("toggl_project_id"):
            project_id = int(ref["toggl_project_id"])
        if ref.get("description"):
            description = ref["description"]
        _lg().info("RESUMED source=device: replaying stored Toggl context "
                   "project_id=%s description=%r thing=%s",
                   project_id, description, thing_name)
    else:
        _lg().info("RESUMED source=device: no Toggl context; falling back to "
                   "secret defaults thing=%s", thing_name)

    entry = toggl_client.start_entry(
        api_token,
        workspace_id=ws_id,
        project_id=project_id,
        description=description,
    )
    entry_id = int(entry["id"])
    state_store.set_running_entry(thing_name, entry_id)
    _lg().info("RESUMED source=device: started Toggl entry %d for %s",
               entry_id, thing_name)
    _push_task_name_for_project(thing_name, api_token, ws_id, project_id)


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
            _lg().info("No running Toggl entry to stop for %s", thing_name)
            return

    toggl_client.stop_entry(api_token, workspace_id=ws_id, entry_id=entry_id)
    state_store.clear_running_entry(thing_name)
    _lg().info("Stopped Toggl entry %d for %s", entry_id, thing_name)


# Dispatch table — adding a new device.session.* mapping is one line.
HANDLERS = {
    ev.DEVICE_WORK_STARTED:    _on_work_started,
    # NOT _on_work_started — RESUMED must never create a fresh Toggl entry.
    # See _on_work_resumed for the stale-shadow-delta scenario.
    ev.DEVICE_WORK_RESUMED:    _on_work_resumed,
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

    logger = logctx.bind(
        log,
        aws_request_id=logctx.request_id(context),
        event_id=detail.get("event_id"),
        shadow_version=detail.get("shadow_version"),
        extra={
            "thing": thing_name,
            "dt":    detail_type,
            "src":   detail.get("state_change_source"),
        },
    )

    logger.info("toggl_api detail=%s", json.dumps(detail)[:200])

    if not detail_type or not thing_name:
        logger.warning("Missing detail-type or thing_name; ignoring")
        return {"ok": False, "reason": "malformed"}

    action = HANDLERS.get(detail_type)
    if not action:
        logger.info("No action mapped for %s; skipping", detail_type)
        return {"ok": True, "skipped": detail_type}

    # Stash the logger on a thread-safe-ish module variable so the per-event
    # handler functions can access it without passing an extra arg through
    # every function signature. Lambda warm-restarts run handler() one event
    # at a time so there's no concurrency to race with.
    global _current_logger
    _current_logger = logger
    try:
        action(thing_name, detail)
    finally:
        _current_logger = log  # back to base for any out-of-band logs

    return {"ok": True, "handled": detail_type}


# Default to the module logger until handler() binds the request scope.
_current_logger = log


def _lg():
    """Accessor so per-event helpers pick up the request-scoped adapter."""
    return _current_logger
