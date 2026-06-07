"""Project-aware decision matrix + sequence-poll for consumer-device-shadow."""

from __future__ import annotations
import io
import json
from unittest.mock import MagicMock, patch

import pytest

# Patch boto3 before import — module instantiates iot-data at import time.
with patch("boto3.client") as _mock_boto:
    _mock_boto.return_value = MagicMock()
    from consumers.device_shadow import handler as device_shadow

from shared import events as ev

THING = "M5StackCore2"


def _shadow_payload(device_state: str, session_type: str = "work",
                    task_name: str = "",
                    command_id: str = "") -> dict:
    body = {"state": {"reported": {
        "device_state":  device_state,
        "session_type":  session_type,
        "task_name":     task_name,
        "command_id":    command_id,
    }}}
    return {"payload": io.BytesIO(json.dumps(body).encode())}


def _event(detail_type: str, *, project_name: str = "",
           description: str = "", event_id: str = "evt-1") -> dict:
    return {
        "detail-type": detail_type,
        "detail": {
            "thing_name":     THING,
            "event_id":       event_id,
            "timestamp":      1,
            "toggl_entry_id": 0,
            "project_name":   project_name or None,
            "description":    description or None,
        },
    }


@pytest.fixture(autouse=True)
def _no_real_sleep(monkeypatch):
    """The sequence-poll calls time.sleep; avoid real waits in tests."""
    monkeypatch.setattr(device_shadow.time, "sleep", lambda _s: None)


# ---------------------------------------------------------------------------
# Bug 1 (paused-stalled) is the regression to defend most carefully.
# ---------------------------------------------------------------------------

def test_paused_same_project_resumes():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(return_value=_shadow_payload(
        "paused", task_name="Learning Networking"))
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(
        _event(ev.EXTERNAL_TOGGL_STARTED, project_name="Learning Networking"),
        None,
    )
    assert resp["command"] == "resume"
    body = json.loads(iot.update_thing_shadow.call_args.kwargs["payload"])
    assert body["state"]["desired"]["command"] == "resume"


def test_paused_different_project_stops_then_starts(monkeypatch):
    iot = device_shadow._iot_data
    # First call: paused. Subsequent calls (ack poll + after-step reads):
    # device echoes the step's command_id so the wait returns True.
    iot.get_thing_shadow = MagicMock(side_effect=[
        _shadow_payload("paused", task_name="Old Project"),
        _shadow_payload("idle", command_id="evt-2#0#stop"),  # ack of step 0
    ])
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(
        _event(ev.EXTERNAL_TOGGL_STARTED,
               project_name="New Project", event_id="evt-2"),
        None,
    )
    assert resp["sequence"] == ["stop", "start"]
    # Two desired-command writes
    cmds = [json.loads(c.kwargs["payload"])["state"]["desired"]["command"]
            for c in iot.update_thing_shadow.call_args_list]
    assert cmds == ["stop", "start"]


# ---------------------------------------------------------------------------
# idle/cold start
# ---------------------------------------------------------------------------

def test_idle_starts():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(return_value=_shadow_payload("idle"))
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(
        _event(ev.EXTERNAL_TOGGL_STARTED, project_name="Whatever"), None,
    )
    assert resp["command"] == "start"


# ---------------------------------------------------------------------------
# active(work)
# ---------------------------------------------------------------------------

def test_active_work_same_project_is_noop():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(return_value=_shadow_payload(
        "active", task_name="Same Project"))
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(
        _event(ev.EXTERNAL_TOGGL_STARTED, project_name="Same Project"), None,
    )
    assert resp.get("skipped") == "already_running_same_project"
    iot.update_thing_shadow.assert_not_called()


def test_active_work_different_project_stops_then_starts():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(side_effect=[
        _shadow_payload("active", task_name="Old"),
        _shadow_payload("idle", command_id="evt-3#0#stop"),
    ])
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(
        _event(ev.EXTERNAL_TOGGL_STARTED,
               project_name="New", event_id="evt-3"),
        None,
    )
    assert resp["sequence"] == ["stop", "start"]


# ---------------------------------------------------------------------------
# break states left alone
# ---------------------------------------------------------------------------

def test_break_active_skip_then_start_on_toggl_start():
    """User wants to bail out of the break early and start the next work
    session — skip+start is the way to do it without screwing up the
    pomodoro cycle accounting."""
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(side_effect=[
        _shadow_payload("active", session_type="short_break"),
        _shadow_payload("idle", command_id="evt-skip#0#skip"),  # ack of skip
    ])
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(
        _event(ev.EXTERNAL_TOGGL_STARTED,
               project_name="Next thing", event_id="evt-skip"),
        None,
    )
    assert resp["sequence"] == ["skip", "start"]
    cmds = [json.loads(c.kwargs["payload"])["state"]["desired"]["command"]
            for c in iot.update_thing_shadow.call_args_list]
    assert cmds == ["skip", "start"]


def test_long_break_also_skips_then_starts():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(side_effect=[
        _shadow_payload("active", session_type="long_break"),
        _shadow_payload("idle", command_id="evt-lb#0#skip"),
    ])
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(
        _event(ev.EXTERNAL_TOGGL_STARTED,
               project_name="Whatever", event_id="evt-lb"),
        None,
    )
    assert resp["sequence"] == ["skip", "start"]


def test_break_active_not_paused_on_toggl_stop():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(return_value=_shadow_payload(
        "active", session_type="short_break"))
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(_event(ev.EXTERNAL_TOGGL_STOPPED), None)
    assert "no_action_in_state" in resp.get("skipped", "")
    iot.update_thing_shadow.assert_not_called()


# ---------------------------------------------------------------------------
# stop
# ---------------------------------------------------------------------------

def test_active_work_pauses_on_stop():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(return_value=_shadow_payload("active"))
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(_event(ev.EXTERNAL_TOGGL_STOPPED), None)
    assert resp["command"] == "pause"
    body = json.loads(iot.update_thing_shadow.call_args.kwargs["payload"])
    assert body["state"]["desired"]["command"] == "pause"
    # The "nuke reported.command so the verb is always in the delta" trick
    assert body["state"]["reported"]["command"] is None


def test_idle_stop_is_noop():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(return_value=_shadow_payload("idle"))
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(_event(ev.EXTERNAL_TOGGL_STOPPED), None)
    assert "no_action_in_state" in resp.get("skipped", "")
    iot.update_thing_shadow.assert_not_called()


# ---------------------------------------------------------------------------
# project comparison falls back to description when no project_name is set
# ---------------------------------------------------------------------------

def test_no_project_name_uses_description_as_label():
    iot = device_shadow._iot_data
    iot.get_thing_shadow = MagicMock(return_value=_shadow_payload(
        "paused", task_name="Ad-hoc thing"))
    iot.update_thing_shadow = MagicMock()

    resp = device_shadow.handler(
        _event(ev.EXTERNAL_TOGGL_STARTED, description="Ad-hoc thing"), None,
    )
    assert resp["command"] == "resume"
