"""Convert a shadow update document into a list of EventBridge events.

The IoT Topic Rule on `$aws/things/+/shadow/update/documents` delivers both
the previous and current shadow snapshots in one payload. This module
diffs them and emits one or more atomic `device.session.*` events covering
the state-machine transitions that happened.

A single shadow update can carry MULTIPLE transitions (e.g. work session
completed -> sequence advanced -> next session became "active" -> all in
one shadow document). The parser emits one event per logical transition;
consumers' rule filters slot them where they belong.
"""

from __future__ import annotations
from typing import Any, Optional

from . import events as ev


def _reported(snap: dict) -> dict:
    """Safely extract state.reported from a shadow snapshot dict."""
    if not snap:
        return {}
    return (snap.get("state") or {}).get("reported") or {}


def parse(thing_name: str, document: dict) -> list[dict]:
    """Return a list of PutEventsEntry dicts derived from a shadow
    `update/documents` payload.

    `document` is the structure AWS IoT delivers — see
    https://docs.aws.amazon.com/iot/latest/developerguide/device-shadow-mqtt.html#update-documents-pub-sub-topic
        {
          "previous": {"state": {"reported": {...}}, "metadata": {...}, "version": N},
          "current":  {"state": {"reported": {...}}, "metadata": {...}, "version": N+1},
          "timestamp": <epoch s>
        }
    """
    prev = _reported(document.get("previous") or {})
    cur  = _reported(document.get("current")  or {})
    if not cur:
        return []  # nothing reported yet; nothing to do

    out: list[dict] = []

    # Server-side ingest time, in seconds. AWS IoT stamps this when it
    # processes the shadow update — only milliseconds after the device
    # publishes. We use it as the authoritative session time everywhere
    # downstream (notably GCal entry windows) because the BM8563 RTC on
    # the device drifts a few seconds per day between NTP syncs and is
    # not a safe wall-clock source. The device's own `reported.timestamp`
    # and `reported.last_event_at` are kept in the payload schema only
    # so legacy callers don't choke; nothing computes from them anymore.
    ts: int = int(document.get("timestamp") or 0)
    if not ts:
        # Defensive fallback for the never-supposed-to-happen case where
        # AWS omits the document timestamp.
        import time as _time
        ts = int(_time.time())

    session_type   = cur.get("session_type")
    session_number = cur.get("session_number")
    total_sessions = cur.get("total_sessions")
    duration_min   = cur.get("duration_min")
    today          = cur.get("today")
    week           = cur.get("week")
    lifetime       = cur.get("lifetime")
    task_name      = cur.get("task_name") or None

    def _build(detail_type: str, *,
               ts_override: Optional[int] = None) -> dict:
        # All transitions get the server-side ingest time. ts_override is
        # accepted only as a no-op for the call-sites kept for clarity;
        # we deliberately do *not* honour device-reported `last_event_at`
        # because that's what was making GCal entries drift by minutes.
        timestamp = ts
        _ = ts_override  # intentionally unused
        detail = ev.DeviceSessionDetail(
            thing_name=thing_name,
            timestamp=timestamp,
            session_type=session_type,
            session_number=session_number,
            total_sessions=total_sessions,
            duration_min=duration_min,
            today=today,
            week=week,
            lifetime=lifetime,
            task_name=task_name,
            event_id=ev.synth_event_id(thing_name, timestamp, detail_type),
        )
        return ev.device_event(detail_type, detail)

    # --- device_state / session_type transitions ----------------------------
    # Most transitions cross a device_state boundary (idle↔active, active↔paused).
    # The exception is work→break, which keeps device_state="active" the whole
    # time and only flips session_type — easy to miss if you gate everything on
    # the state diff.
    prev_state        = prev.get("device_state")
    cur_state         = cur.get("device_state")
    prev_session_type = prev.get("session_type")

    # work_started / work_resumed: anything → active/work
    if (cur_state == "active" and session_type == "work"
            and prev_state != "active"):
        if prev_state == "paused":
            out.append(_build(ev.DEVICE_WORK_RESUMED))
        else:  # idle, None, or any unexpected previous state
            out.append(_build(ev.DEVICE_WORK_STARTED))

    # work_paused: active/work → paused
    elif cur_state == "paused" and prev_state == "active":
        out.append(_build(ev.DEVICE_WORK_PAUSED))

    # break_started: active/work → active/break (device_state unchanged, only
    # session_type flips). Anchor on session_type changing while we stay in
    # active so a stale "no transition" shadow republish doesn't refire it.
    if (cur_state == "active"
            and session_type in ("short_break", "long_break")
            and prev_session_type == "work"):
        out.append(_build(ev.DEVICE_BREAK_STARTED))

    # --- completion events (last_event) -------------------------------------
    # last_event_at is set when the device records a session-boundary event.
    # We treat its appearance/change as the source of truth for completions,
    # not the device_state delta, because by the time the shadow is published
    # the device has already advanced through the IDLE state and back into
    # the next session — the state delta alone would miss "work finished."
    prev_event_at = prev.get("last_event_at") or 0
    cur_event_at  = cur.get("last_event_at")  or 0
    if cur_event_at and cur_event_at != prev_event_at:
        last_event = cur.get("last_event")
        if last_event == "work_complete":
            out.append(_build(ev.DEVICE_WORK_COMPLETED, ts_override=cur_event_at))
        elif last_event == "break_complete":
            out.append(_build(ev.DEVICE_BREAK_COMPLETED, ts_override=cur_event_at))
        elif last_event == "cycle_complete":
            out.append(_build(ev.DEVICE_CYCLE_COMPLETED, ts_override=cur_event_at))

    # --- wake (boot / sleep wake) ------------------------------------------
    # wake_id is a per-boot random value the firmware stamps in every reported
    # snapshot. A change means the device just came online; we synthesise a
    # device.wake event so consumer-wake-resync can resync the Toggl context.
    prev_wake = prev.get("wake_id")
    cur_wake  = cur.get("wake_id")
    if cur_wake is not None and cur_wake != prev_wake:
        wake_detail = ev.DeviceWakeDetail(
            thing_name=thing_name,
            timestamp=ts,
            event_id=ev.synth_event_id(thing_name, ts, ev.DEVICE_WAKE),
            wake_id=int(cur_wake),
            device_state=cur.get("device_state"),
            session_type=session_type,
            session_number=session_number,
            total_sessions=total_sessions,
            duration_min=duration_min,
            task_name=cur.get("task_name"),
            today=today,
            week=week,
            lifetime=lifetime,
        )
        out.append(ev.device_wake_event(wake_detail))

    return out
