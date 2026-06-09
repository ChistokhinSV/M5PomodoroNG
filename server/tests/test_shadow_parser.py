"""Diff-to-events tests for shared.shadow_parser.

Every row of the state-transition table in the plan corresponds to a test
here. If a future shadow schema change drops a field, the failing test
points at the right line in the parser.
"""

from shared import shadow_parser, events as ev

THING = "M5StackCore2"


def _doc(prev_reported, cur_reported, *, timestamp=1700000000):
    return {
        "previous": {"state": {"reported": prev_reported}} if prev_reported else None,
        "current":  {"state": {"reported": cur_reported}}  if cur_reported  else None,
        "timestamp": timestamp,
    }


def _types(entries):
    return [e["DetailType"] for e in entries]


def test_no_current_yields_no_events():
    assert shadow_parser.parse(THING, _doc({}, {})) == []


def test_first_ever_start_emits_work_started():
    prev = {"device_state": "idle"}
    cur  = {"device_state": "active", "session_type": "work", "timestamp": 1, "duration_min": 25}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert _types(out) == [ev.DEVICE_WORK_STARTED]


def test_first_state_change_with_no_previous():
    # Brand-new device: previous reported is absent. Treat as starting state.
    cur = {"device_state": "active", "session_type": "work", "timestamp": 1}
    out = shadow_parser.parse(THING, _doc(None, cur))
    assert _types(out) == [ev.DEVICE_WORK_STARTED]


def test_active_to_paused_emits_paused():
    prev = {"device_state": "active", "session_type": "work"}
    cur  = {"device_state": "paused", "session_type": "work", "timestamp": 2}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert _types(out) == [ev.DEVICE_WORK_PAUSED]


def test_paused_to_active_emits_resumed():
    prev = {"device_state": "paused", "session_type": "work"}
    cur  = {"device_state": "active", "session_type": "work", "timestamp": 3}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert _types(out) == [ev.DEVICE_WORK_RESUMED]


def test_active_work_to_active_break_emits_break_started():
    prev = {"device_state": "active", "session_type": "work"}
    cur  = {"device_state": "active", "session_type": "short_break", "timestamp": 4}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert _types(out) == [ev.DEVICE_BREAK_STARTED]


def test_last_event_work_complete_emits_work_completed():
    prev = {"device_state": "active", "session_type": "work", "last_event_at": 0}
    cur  = {"device_state": "idle",   "session_type": "work",
            "last_event": "work_complete", "last_event_at": 100,
            "session_number": 2, "total_sessions": 4, "duration_min": 25}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    # State went active->idle (not in the device_state branch) but
    # last_event_at changed and last_event is work_complete -> emit completed.
    types = _types(out)
    assert ev.DEVICE_WORK_COMPLETED in types


def test_last_event_cycle_complete_emits_cycle_completed():
    prev = {"last_event_at": 100}
    cur  = {"last_event": "cycle_complete", "last_event_at": 200,
            "today": 8}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert _types(out) == [ev.DEVICE_CYCLE_COMPLETED]


def test_same_last_event_at_no_event():
    # If only metadata changed but no new completion event, no events.
    prev = {"last_event_at": 100, "last_event": "work_complete"}
    cur  = {"last_event_at": 100, "last_event": "work_complete"}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert out == []


def test_event_id_is_deterministic():
    # Same transition replayed produces same event_id, which consumers
    # depend on for idempotency.
    prev = {"device_state": "idle"}
    cur  = {"device_state": "active", "session_type": "work", "timestamp": 7}
    first  = shadow_parser.parse(THING, _doc(prev, cur))
    second = shadow_parser.parse(THING, _doc(prev, cur))

    import json
    first_id  = json.loads(first[0]["Detail"])["event_id"]
    second_id = json.loads(second[0]["Detail"])["event_id"]
    assert first_id == second_id


def test_completion_uses_document_timestamp_not_device_clock():
    # We deliberately ignore the device-reported `last_event_at` and use
    # the AWS-stamped `document.timestamp` instead. The BM8563 RTC on the
    # device drifts a few seconds per day between NTP syncs, so trusting
    # last_event_at would let GCal entries skew over time. The AWS-side
    # ingest stamp is milliseconds away from when the device actually
    # finished the session and is rock-stable.
    prev = {"last_event_at": 0}
    cur  = {"last_event": "work_complete", "last_event_at": 12345}
    out = shadow_parser.parse(THING, _doc(prev, cur, timestamp=99999))

    import json
    detail = json.loads(out[0]["Detail"])
    assert detail["timestamp"] == 99999


# --- state-changed catchup (offline-transition recovery) -----------------

def test_state_changed_catchup_emits_work_started():
    """Real device boot log scenario: user pressed Start before WiFi/MQTT
    came up. By the time the first shadow snapshot lands, shadow's prior
    reported.device_state (from a previous session) already matches the
    new "active" — so the device_state diff branch misses the WORK_STARTED.
    The firmware's explicit STATE_CHANGED event update has a fresh
    last_event_at + last_event='state_changed', which the catchup uses to
    synthesise the event."""
    prev = {"device_state": "active", "session_type": "work", "last_event_at": 100}
    cur  = {"device_state": "active", "session_type": "work",
            "last_event": "state_changed", "last_event_at": 200}
    out = shadow_parser.parse(THING, _doc(prev, cur, timestamp=300))
    assert _types(out) == [ev.DEVICE_WORK_STARTED]


def test_state_changed_catchup_paused():
    """Same offline-transition recovery, but device ended up PAUSED."""
    prev = {"device_state": "paused", "session_type": "work", "last_event_at": 100}
    cur  = {"device_state": "paused", "session_type": "work",
            "last_event": "state_changed", "last_event_at": 200}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert _types(out) == [ev.DEVICE_WORK_PAUSED]


def test_state_changed_catchup_idle_is_ambiguous_so_no_event():
    """cur_state=idle could mean stop, cycle reset, cycle complete — too
    ambiguous to invent a device.session.* event. Leave it alone."""
    prev = {"device_state": "idle", "last_event_at": 100}
    cur  = {"device_state": "idle",
            "last_event": "state_changed", "last_event_at": 200}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert out == []


def test_state_changed_catchup_skipped_when_diff_already_emitted():
    """Don't double-emit: when the device_state diff branch already fired
    WORK_STARTED (e.g. idle -> active), the catchup must not also fire."""
    prev = {"device_state": "idle", "last_event_at": 100}
    cur  = {"device_state": "active", "session_type": "work",
            "last_event": "state_changed", "last_event_at": 200}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert _types(out) == [ev.DEVICE_WORK_STARTED]   # exactly one


def test_state_changed_catchup_skipped_when_event_at_unchanged():
    """No new transition signal -> no synthesis."""
    prev = {"device_state": "active", "session_type": "work",
            "last_event": "state_changed", "last_event_at": 200}
    cur  = {"device_state": "active", "session_type": "work",
            "last_event": "state_changed", "last_event_at": 200}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert out == []


# --- wake_id detection ----------------------------------------------------

def test_wake_id_change_emits_device_wake():
    prev = {"wake_id": 11111}
    cur  = {"wake_id": 22222, "device_state": "idle", "duration_min": 25}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    types = _types(out)
    assert ev.DEVICE_WAKE in types

    import json
    wake = [e for e in out if e["DetailType"] == ev.DEVICE_WAKE][0]
    detail = json.loads(wake["Detail"])
    assert detail["wake_id"] == 22222
    assert detail["duration_min"] == 25
    assert detail["device_state"] == "idle"


def test_first_wake_id_with_no_previous_also_emits():
    # Brand-new shadow with no prev. wake_id appears for the first time -> wake.
    cur = {"wake_id": 33333, "device_state": "idle"}
    out = shadow_parser.parse(THING, _doc(None, cur))
    assert ev.DEVICE_WAKE in _types(out)


def test_same_wake_id_no_event():
    prev = {"wake_id": 4444}
    cur  = {"wake_id": 4444, "device_state": "active"}
    out = shadow_parser.parse(THING, _doc(prev, cur))
    assert ev.DEVICE_WAKE not in _types(out)
