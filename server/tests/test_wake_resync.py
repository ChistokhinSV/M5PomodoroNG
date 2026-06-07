"""Modular-offset math + skip logic for consumer-wake-resync."""

from __future__ import annotations
import io
import json
from unittest.mock import MagicMock, patch

import pytest

# Patch boto3 before import — module instantiates iot-data at import time.
with patch("boto3.client") as _mock_boto:
    _mock_boto.return_value = MagicMock()
    from consumers.wake_resync import handler as wake_resync


THING = "M5StackCore2"


@pytest.fixture(autouse=True)
def _patch_secrets(monkeypatch):
    monkeypatch.setattr(
        wake_resync.sec, "get_secret",
        lambda _: {"api_token": "tok", "workspace_id": 99},
    )


@pytest.fixture(autouse=True)
def _patch_state_store(monkeypatch):
    monkeypatch.setattr(wake_resync.state_store, "set_task_context",
                        lambda *a, **kw: None)


@pytest.fixture
def fake_toggl(monkeypatch):
    fake = MagicMock()
    fake.current_entry.return_value = None
    fake.get_project.return_value = None
    monkeypatch.setattr(wake_resync, "toggl_client", fake)
    return fake


@pytest.fixture
def fake_time(monkeypatch):
    """Pin time.time() to a known value so the math is deterministic."""
    monkeypatch.setattr(wake_resync.time, "time", lambda: 1_780_000_000)
    return 1_780_000_000


def _event(*, device_state="idle", duration_min=25, event_id="wake-1") -> dict:
    return {
        "detail-type": "device.wake",
        "detail": {
            "thing_name":     THING,
            "event_id":       event_id,
            "timestamp":      1,
            "wake_id":        12345,
            "device_state":   device_state,
            "duration_min":   duration_min,
        },
    }


# ---------------------------------------------------------------------------
# skip cases
# ---------------------------------------------------------------------------

def test_active_device_is_not_disturbed(fake_toggl, fake_time):
    iot = wake_resync._iot_data
    iot.update_thing_shadow = MagicMock()

    resp = wake_resync.handler(_event(device_state="active"), None)
    assert "skipped" in resp
    iot.update_thing_shadow.assert_not_called()
    fake_toggl.current_entry.assert_not_called()


def test_no_running_toggl_entry_no_op(fake_toggl, fake_time):
    iot = wake_resync._iot_data
    iot.update_thing_shadow = MagicMock()
    fake_toggl.current_entry.return_value = None  # explicit

    resp = wake_resync.handler(_event(), None)
    assert resp["skipped"] == "no_running_entry"
    iot.update_thing_shadow.assert_not_called()


# ---------------------------------------------------------------------------
# math
# ---------------------------------------------------------------------------

def _started_seconds_ago(seconds: int, *, project_id=None, description=None) -> dict:
    """Build a Toggl current_entry dict whose start is N seconds before
    the pinned time.time() = 1_780_000_000."""
    from datetime import datetime, timezone
    start_dt = datetime.fromtimestamp(1_780_000_000 - seconds, tz=timezone.utc)
    return {
        "id": 42,
        "start": start_dt.isoformat().replace("+00:00", "Z"),
        "project_id": project_id,
        "description": description,
        "tags": [],
    }


def test_simple_offset_25min_running_10min_yields_15min(fake_toggl, fake_time):
    """25-min interval, Toggl running 10 min -> remaining 15 min (= 900 s)."""
    fake_toggl.current_entry.return_value = _started_seconds_ago(10 * 60)
    iot = wake_resync._iot_data
    iot.update_thing_shadow = MagicMock()

    resp = wake_resync.handler(_event(duration_min=25), None)
    assert resp["remaining_sec"] == 15 * 60
    body = json.loads(iot.update_thing_shadow.call_args.kwargs["payload"])
    assert body["state"]["desired"]["command"] == "start"
    assert body["state"]["desired"]["remaining_sec_override"] == 15 * 60


def test_modular_overflow_37min_into_10min_yields_3min(fake_toggl, fake_time):
    """10-min interval, Toggl running 37 min -> 37 % 10 = 7 -> remaining 3 min."""
    fake_toggl.current_entry.return_value = _started_seconds_ago(37 * 60)

    resp = wake_resync.handler(_event(duration_min=10), None)
    assert resp["remaining_sec"] == 3 * 60


def test_tiny_remainder_skips_to_full_interval(fake_toggl, fake_time, monkeypatch):
    """25-min interval, Toggl running 24m58s -> 2s remaining -> bumps to 25 min."""
    monkeypatch.setattr(wake_resync, "TINY_REMAINDER_S", 30)
    fake_toggl.current_entry.return_value = _started_seconds_ago(25 * 60 - 2)

    resp = wake_resync.handler(_event(duration_min=25), None)
    assert resp["remaining_sec"] == 25 * 60  # full duration


def test_exactly_one_interval_in_yields_full_again(fake_toggl, fake_time):
    """10-min interval, Toggl running exactly 10 min -> remainder 0 -> full
    duration (10 min) is the boundary case the modulo handles."""
    fake_toggl.current_entry.return_value = _started_seconds_ago(10 * 60)

    resp = wake_resync.handler(_event(duration_min=10), None)
    assert resp["remaining_sec"] == 10 * 60


# ---------------------------------------------------------------------------
# task_name resolution
# ---------------------------------------------------------------------------

def test_uses_project_name_when_available(fake_toggl, fake_time):
    fake_toggl.current_entry.return_value = _started_seconds_ago(
        5 * 60, project_id=4242,
    )
    fake_toggl.get_project.return_value = {"id": 4242, "name": "Side Quest"}

    resp = wake_resync.handler(_event(duration_min=25), None)
    assert resp["task_name"] == "Side Quest"
    body = json.loads(wake_resync._iot_data.update_thing_shadow.call_args.kwargs["payload"])
    assert body["state"]["desired"]["task_name"] == "Side Quest"


def test_falls_back_to_description_when_no_project(fake_toggl, fake_time):
    fake_toggl.current_entry.return_value = _started_seconds_ago(
        5 * 60, project_id=None, description="Quick fix",
    )
    resp = wake_resync.handler(_event(duration_min=25), None)
    assert resp["task_name"] == "Quick fix"
