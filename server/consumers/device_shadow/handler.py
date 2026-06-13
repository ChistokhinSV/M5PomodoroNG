"""consumer-device-shadow Lambda.

Trigger: EventBridge rule matching `detail-type` startswith `external.toggl.`.

Translates Toggl-side timer events into AWS IoT shadow `desired.command`
updates. The firmware's `ShadowPublisher::handleShadowDelta` then dispatches
into `TimerStateMachine::handleEvent(...)` and echoes the command back into
`reported`, which clears the delta.

Behavior matrix (project-aware):

  toggl.timer.started + device idle                  -> start
  toggl.timer.started + paused, same project         -> resume
  toggl.timer.started + paused, different project    -> stop, then start
  toggl.timer.started + active(work), same project   -> no-op
  toggl.timer.started + active(work), other project  -> stop, then start
  toggl.timer.started + active(break)                -> skip the break,
                                                       then start the next
                                                       work session early
  toggl.timer.stopped + active work                  -> pause (kept as pause so
                                                       a re-start on the same
                                                       project resumes the
                                                       same pomodoro session)
  toggl.timer.stopped + any other state              -> no-op

"Same project" is decided by comparing the incoming event's project_name
(or description) against the device's current reported.task_name. That's
the only authoritative "what is the device tracking right now" — using
the device shadow avoids racing with consumer-task-context's DDB write.
"""

from __future__ import annotations
import json
import logging
import os
import time

import boto3

from shared import events as ev
from shared import logctx
from shared import state_store

log = logging.getLogger()
log.setLevel(logging.INFO)

REGION = os.environ.get("AWS_REGION", "eu-central-1")

# iot-data client speaks the shadow REST API. iot client (used elsewhere)
# is for control-plane operations like thing/cert management — wrong client
# for shadows.
_iot_data = boto3.client("iot-data", region_name=REGION)


def _get_reported_state(thing_name: str, logger) -> dict:
    try:
        resp = _iot_data.get_thing_shadow(thingName=thing_name)
    except _iot_data.exceptions.ResourceNotFoundException:
        logger.warning("No shadow yet for thing=%s", thing_name)
        return {}
    payload = json.loads(resp["payload"].read())
    return (payload.get("state") or {}).get("reported") or {}


def _publish_desired_command(thing_name: str, command: str, command_id: str) -> None:
    """Write a desired.command + command_id, while *also* clearing
    reported.command in the same shadow update.

    Why nuke reported.command? AWS computes shadow deltas field-by-field:
    if reported.command already equals desired.command (e.g. "start" left
    over from a previous interaction), the next "start" write produces a
    delta that lacks the command field, and the device has nothing to
    dispatch on. By setting reported.command=null in the same call, we
    guarantee desired.command != reported.command, so the delta always
    carries the verb. The device's normal echo on action restores
    reported.command after handleEvent runs.
    """
    body = {
        "state": {
            "desired":  {"command": command, "command_id": command_id},
            "reported": {"command": None},
        }
    }
    _iot_data.update_thing_shadow(
        thingName=thing_name,
        payload=json.dumps(body).encode("utf-8"),
    )


def _incoming_task_name(detail: dict) -> str:
    """Pick the user-facing label out of the incoming event the same way
    consumer-task-context does. Used here to compare against the device's
    current reported.task_name without touching DDB (avoids racing with
    the task-context consumer's write)."""
    name = (detail.get("project_name") or detail.get("description") or "").strip()
    return name


def _wait_for_command_ack(thing_name: str, command_id: str, logger,
                          timeout_s: float = 4.0) -> bool:
    """Poll shadow.reported.command_id until it matches what we just
    published, or until timeout. Used to serialize multi-step command
    sequences (stop -> start) so AWS doesn't coalesce them into one delta
    and drop the intermediate verb on the floor."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        time.sleep(0.25)
        try:
            reported = _get_reported_state(thing_name, logger)
        except Exception as exc:
            logger.warning("ack-poll get_thing_shadow failed: %s", exc)
            continue
        if reported.get("command_id") == command_id:
            return True
    return False


def _publish_sequence(thing_name: str, commands: list[str],
                      event_id_base: str, logger) -> dict:
    """Push a sequence of desired commands, waiting for each device ack
    before sending the next. Each step gets a derived command_id so the
    firmware can tell them apart."""
    for i, cmd in enumerate(commands):
        cmd_id = f"{event_id_base}#{i}#{cmd}"
        _publish_desired_command(thing_name, cmd, cmd_id)
        logger.info("Sequence step %d/%d: command=%s id=%s",
                    i + 1, len(commands), cmd, cmd_id)
        if i < len(commands) - 1:
            acked = _wait_for_command_ack(thing_name, cmd_id, logger)
            if not acked:
                logger.warning("Device didn't ack '%s' (id=%s) within timeout; "
                               "sending next step anyway", cmd, cmd_id)
    return {"ok": True, "sequence": commands}


def _previous_task_name(thing_name: str) -> str:
    """Return the device's last-seen project name from DDB task_context.

    We deliberately do NOT read reported.task_name from the device shadow:
    the parallel consumer-task-context Lambda nullifies reported.task_name
    in the very same shadow update where it writes desired.task_name (the
    null trick that lets a freshly-booted device receive the field). If
    we read the shadow here we race with that clear and can briefly see
    None, which makes same_project return False for a project that
    didn't actually change — the device then suffers a stop+start it
    shouldn't.

    DDB's task_context row is the authoritative "what project was the
    device tracking last". consumer-task-context writes it AFTER its
    shadow publish, so reading it here naturally gives us the value from
    the PREVIOUS Toggl event — which is exactly what we need for the
    same_project comparison."""
    ctx = state_store.get_task_context(thing_name)
    if not ctx:
        return ""
    return (ctx.get("task_name") or "").strip()


def _handle_toggl_start(thing_name: str, detail: dict, event_id: str,
                        logger) -> dict:
    """Project-aware start. Resume same project, restart (stop+start) on
    project change, leave breaks alone."""
    reported = _get_reported_state(thing_name, logger)
    state = reported.get("device_state")
    session = reported.get("session_type")
    current_name = _previous_task_name(thing_name)
    incoming_name = _incoming_task_name(detail)
    same_project = bool(incoming_name) and incoming_name == current_name

    logger.info("toggl.start state=%s session=%s incoming=%r current=%r same=%s",
                state, session, incoming_name, current_name, same_project)

    if state == "idle":
        _publish_desired_command(thing_name, "start", event_id)
        return {"ok": True, "command": "start", "reason": "idle"}

    if state == "paused":
        if same_project:
            _publish_desired_command(thing_name, "resume", event_id)
            return {"ok": True, "command": "resume", "reason": "same_project"}
        # Different (or unknown) project -> reset and start fresh.
        return _publish_sequence(thing_name, ["stop", "start"], event_id, logger)

    if state == "active" and session == "work":
        if same_project:
            return {"ok": True, "skipped": "already_running_same_project"}
        return _publish_sequence(thing_name, ["stop", "start"], event_id, logger)

    if state == "active" and session in ("short_break", "long_break"):
        # User started a Toggl timer mid-break — they're ready to work again.
        # SKIP advances the sequence to the next work session and stops the
        # break timer; START kicks off that next work interval. Project
        # comparison isn't useful here (no work session to compare against),
        # the rendered task_name comes from the parallel task-context update.
        return _publish_sequence(thing_name, ["skip", "start"], event_id, logger)

    return {"ok": True, "skipped": f"no_action_in_state={state}/{session}"}


def _handle_toggl_stop(thing_name: str, detail: dict, event_id: str,
                       logger) -> dict:
    """Toggl stop pauses an in-flight work pomodoro. Stays a pause (not a
    full stop) so a re-start on the same project resumes the same session."""
    reported = _get_reported_state(thing_name, logger)
    state = reported.get("device_state")
    session = reported.get("session_type")

    if state == "active" and session == "work":
        _publish_desired_command(thing_name, "pause", event_id)
        return {"ok": True, "command": "pause"}
    return {"ok": True, "skipped": f"no_action_in_state={state}/{session}"}


def handler(event: dict, context) -> dict:
    detail_type = event.get("detail-type") or event.get("DetailType")
    detail = event.get("detail") or {}
    thing_name = detail.get("thing_name")
    event_id   = detail.get("event_id")

    logger = logctx.bind(
        log,
        aws_request_id=logctx.request_id(context),
        event_id=event_id,
        extra={"thing": thing_name, "dt": detail_type},
    )

    logger.info("device_shadow detail=%s", json.dumps(detail)[:300])

    if not detail_type or not thing_name or not event_id:
        logger.warning("Missing detail-type / thing_name / event_id")
        return {"ok": False, "reason": "malformed"}

    if detail_type == ev.EXTERNAL_TOGGL_STARTED:
        return _handle_toggl_start(thing_name, detail, event_id, logger)
    if detail_type == ev.EXTERNAL_TOGGL_STOPPED:
        return _handle_toggl_stop(thing_name, detail, event_id, logger)

    logger.info("No handler for detail-type=%s", detail_type)
    return {"ok": True, "skipped": detail_type}
