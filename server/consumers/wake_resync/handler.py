"""consumer-wake-resync Lambda.

Trigger: EventBridge rule matching `detail-type == device.wake`.

When the device boots (fresh wake_id), this consumer fast-forwards the
device's pomodoro timer to match what's been running in Toggl, so the user
doesn't have to manually start anything after waking the M5 to find Toggl
already going. Algorithm:

  1. Skip if device isn't idle (a running pomodoro shouldn't be disturbed
     by the wake event).
  2. Ask Toggl for the user's currently running time entry. None? -> skip.
  3. Compute the modular offset:
        elapsed_sec  = now - entry.start
        duration_sec = reported.duration_min * 60
        remainder    = elapsed_sec % duration_sec
        remaining_sec = duration_sec - remainder
     The modulo handles the "Toggl has been running 37 min into a 10-min
     interval -> 7 min into the 4th interval -> 3 min remaining" case.
  4. If remaining_sec < TINY_REMAINDER_S, skip the current interval and
     give a full one instead (otherwise the device would start a 2-second
     timer that times out before it even draws).
  5. Push `command=start` + `remaining_sec_override` + `task_name` to the
     shadow. Firmware's ShadowPublisher snaps remaining_ms to the override
     value after handleEvent(START) runs.
  6. Mirror the project into DDB task_context so a later device-initiated
     start replays the same project.
"""

from __future__ import annotations
import json
import logging
import os
import time

import boto3

from shared import events as ev
from shared import secrets as sec
from shared import state_store
from shared import toggl_client

log = logging.getLogger()
log.setLevel(logging.INFO)

REGION = os.environ.get("AWS_REGION", "eu-central-1")
TOGGL_SECRET_ARN = os.environ["TOGGL_SECRET_ARN"]
# Below this many seconds remaining, we don't trust the math (the device
# would tick straight into a TIMEOUT). Skip and let the user have a full
# fresh interval. Configurable via the SAM TinyRemainderS parameter.
TINY_REMAINDER_S = int(os.environ.get("TINY_REMAINDER_S", "30"))

_iot_data = boto3.client("iot-data", region_name=REGION)


def _toggl_config() -> tuple[str, int]:
    cfg = sec.get_secret(TOGGL_SECRET_ARN)
    return cfg["api_token"], int(cfg["workspace_id"])


def _entry_start_epoch(entry: dict) -> int | None:
    """Toggl v9 returns 'start' as an ISO-8601 string with Z. Convert to
    epoch seconds. Falls back to 'start_time' if present (older shape)."""
    raw = entry.get("start") or entry.get("start_time")
    if not raw:
        return None
    try:
        # Toggl uses "2026-06-06T10:25:30+00:00" / "...Z".
        from datetime import datetime
        iso = raw.replace("Z", "+00:00")
        return int(datetime.fromisoformat(iso).timestamp())
    except (ValueError, TypeError) as e:
        log.warning("Failed to parse Toggl start time %r: %s", raw, e)
        return None


def _publish_start(thing_name: str, *, command_id: str, remaining_sec: int,
                   task_name: str | None) -> None:
    """Push the start command + override + task_name in one shadow update.
    Same reported.command=null trick as device-shadow so the verb is
    guaranteed to appear in the resulting delta."""
    desired: dict = {
        "command": "start",
        "command_id": command_id,
        "remaining_sec_override": remaining_sec,
    }
    if task_name:
        desired["task_name"] = task_name
    body = {
        "state": {
            "desired":  desired,
            "reported": {"command": None},
        }
    }
    _iot_data.update_thing_shadow(
        thingName=thing_name,
        payload=json.dumps(body).encode("utf-8"),
    )


def handler(event: dict, context) -> dict:
    detail = event.get("detail") or {}
    thing_name = detail.get("thing_name")
    event_id   = detail.get("event_id")
    device_state = (detail.get("device_state") or "").lower()
    duration_min = detail.get("duration_min")

    log.info("wake_resync thing=%s state=%s duration_min=%s event_id=%s",
             thing_name, device_state, duration_min, event_id)

    if not thing_name or not event_id:
        return {"ok": False, "reason": "malformed"}

    if device_state != "idle":
        log.info("Device not idle (state=%s); not resyncing", device_state)
        return {"ok": True, "skipped": f"state={device_state}"}

    if not duration_min or duration_min <= 0:
        log.info("No usable duration_min in wake event; skip")
        return {"ok": True, "skipped": "no_duration_min"}

    # --- 1. Toggl running entry --------------------------------------------
    api_token, workspace_id = _toggl_config()
    try:
        entry = toggl_client.current_entry(api_token)
    except Exception as e:
        log.warning("Toggl current_entry failed: %s", e)
        return {"ok": False, "reason": "toggl_error"}
    if not entry:
        log.info("No running Toggl entry; nothing to resync")
        return {"ok": True, "skipped": "no_running_entry"}

    start_epoch = _entry_start_epoch(entry)
    if start_epoch is None:
        return {"ok": False, "reason": "bad_entry_start"}

    # --- 2. Math -----------------------------------------------------------
    now_epoch = int(time.time())
    elapsed_sec = max(0, now_epoch - start_epoch)
    duration_sec = int(duration_min) * 60
    remainder = elapsed_sec % duration_sec
    remaining_sec = duration_sec - remainder

    log.info("Toggl entry %d running %ds; duration=%ds; remainder=%ds; "
             "remaining_sec=%ds",
             int(entry.get("id", 0)), elapsed_sec, duration_sec, remainder,
             remaining_sec)

    if remaining_sec < TINY_REMAINDER_S:
        # Skip to the next interval with full duration.
        log.info("remaining_sec=%ds below threshold=%ds; using full duration",
                 remaining_sec, TINY_REMAINDER_S)
        remaining_sec = duration_sec

    # --- 3. Resolve project name and mirror into DDB -----------------------
    project_id = entry.get("project_id")
    project_name = None
    if project_id:
        try:
            project = toggl_client.get_project(
                api_token, workspace_id=workspace_id, project_id=int(project_id),
            )
            if project:
                project_name = project.get("name") or None
        except Exception as e:
            log.warning("Project lookup failed: %s", e)
    task_name = project_name or (entry.get("description") or "").strip() or None

    # Mirror context so a later device-initiated start replays the right project.
    if task_name:
        state_store.set_task_context(
            thing_name,
            task_name=task_name,
            provider="toggl",
            provider_ref={
                "toggl_project_id":   int(project_id) if project_id else None,
                "toggl_workspace_id": workspace_id,
                "description":        entry.get("description") or None,
                "tags":               list(entry.get("tags") or []),
            },
        )

    # --- 4. Push start to device -------------------------------------------
    _publish_start(
        thing_name,
        command_id=event_id,            # synthesised from (thing, ts, wake)
        remaining_sec=remaining_sec,
        task_name=task_name,
    )
    log.info("Published start: remaining=%ds task_name=%r", remaining_sec, task_name)

    return {
        "ok": True,
        "remaining_sec": remaining_sec,
        "task_name": task_name,
        "elapsed_sec": elapsed_sec,
    }
