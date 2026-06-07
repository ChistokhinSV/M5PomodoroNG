"""State-machine behavior for consumer-toggl-api."""

from __future__ import annotations
from unittest.mock import MagicMock, patch

import pytest

from shared import events as ev

# Module under test — its import already pulls toggl_client + secrets.
from consumers.toggl_api import handler as toggl_api


THING = "M5StackCore2"


@pytest.fixture(autouse=True)
def _config(monkeypatch):
    monkeypatch.setattr(
        toggl_api.sec, "get_secret",
        lambda _: {
            "toggl": {
                "api_token": "tok",
                "workspace_id": 11,
                "project_id": 22,
                "default_description": "Focus",
            },
        },
    )


@pytest.fixture
def fake_toggl(monkeypatch):
    fake = MagicMock()
    fake.current_entry.return_value = None
    fake.start_entry.return_value = {"id": 999}
    fake.stop_entry.return_value = {"id": 999}
    # Default project lookup is a no-op; tests that care about colour /
    # name flow override .return_value themselves.
    fake.get_project.return_value = None
    monkeypatch.setattr(toggl_api, "toggl_client", fake)
    return fake


@pytest.fixture(autouse=True)
def fake_iot(monkeypatch):
    """Stand-in for the IoT data-plane client. Captures every
    update_thing_shadow payload as a list of (thing_name, parsed-state)
    so tests can assert what task_name / project_color got pushed."""
    fake = MagicMock()
    pushed: list[tuple[str, dict]] = []
    def update(*, thingName, payload):
        import json as _json
        pushed.append((thingName, _json.loads(payload)))
        return {}
    fake.update_thing_shadow.side_effect = update
    monkeypatch.setattr(toggl_api, "_iot_data", fake)
    fake.pushed = pushed
    return fake


@pytest.fixture
def fake_store(monkeypatch):
    """In-memory stand-in for the toggl#running row. Tests access it as
    fake_store[thing] = entry_id, matching the dict shape we keep for
    backwards compatibility with the original tests."""
    state = {}
    def get_running(thing): return state.get(thing)
    def set_running(thing, eid): state[thing] = eid
    def clear_running(thing): state.pop(thing, None)
    monkeypatch.setattr(toggl_api.state_store, "get_running_entry", get_running)
    monkeypatch.setattr(toggl_api.state_store, "set_running_entry", set_running)
    monkeypatch.setattr(toggl_api.state_store, "clear_running_entry", clear_running)
    return state


@pytest.fixture(autouse=True)
def fake_context(monkeypatch):
    """Always-on mock for the task_context lookup so tests that don't care
    about it still hit a no-context default. Tests that want a stored
    context populate ctx_store via the returned dict before calling the
    handler."""
    ctx_store = {}
    def get_task_context(thing): return ctx_store.get(thing)
    monkeypatch.setattr(toggl_api.state_store, "get_task_context", get_task_context)
    return ctx_store


def _event(detail_type, **detail):
    return {
        "detail-type": detail_type,
        "detail": {"thing_name": THING, **detail},
    }


def test_work_started_creates_entry(fake_toggl, fake_store):
    resp = toggl_api.handler(_event(ev.DEVICE_WORK_STARTED), None)
    assert resp["ok"] and resp["handled"] == ev.DEVICE_WORK_STARTED
    fake_toggl.start_entry.assert_called_once()
    assert fake_store[THING] == 999


def test_work_started_adopts_existing_running_entry(fake_toggl, fake_store):
    fake_toggl.current_entry.return_value = {"id": 7777}
    toggl_api.handler(_event(ev.DEVICE_WORK_STARTED), None)
    fake_toggl.start_entry.assert_not_called()
    assert fake_store[THING] == 7777


def test_work_started_uses_stored_task_context(fake_toggl, fake_store, fake_context):
    """When the user has previously started a timer in Toggl on Project X,
    the next device-initiated session should continue Project X (not the
    default project)."""
    fake_context[THING] = {
        "provider": "toggl",
        "task_name": "Side Quest",
        "provider_ref": {
            "toggl_project_id": 4242,
            "description": "Side Quest things",
        },
    }
    toggl_api.handler(_event(ev.DEVICE_WORK_STARTED), None)
    kwargs = fake_toggl.start_entry.call_args.kwargs
    assert kwargs["project_id"] == 4242
    assert kwargs["description"] == "Side Quest things"


def test_work_started_falls_back_when_context_provider_differs(
    fake_toggl, fake_store, fake_context):
    """If the last seen context came from another provider (e.g. Clockify),
    don't reuse its tokens for the Toggl entry; fall back to defaults."""
    fake_context[THING] = {
        "provider": "clockify",
        "task_name": "Doesn't matter",
        "provider_ref": {"clockify_project_id": "abc-123"},
    }
    toggl_api.handler(_event(ev.DEVICE_WORK_STARTED), None)
    kwargs = fake_toggl.start_entry.call_args.kwargs
    assert kwargs["project_id"] == 22        # default from _config fixture
    assert kwargs["description"] == "Focus"  # default from _config fixture


def test_work_completed_stops_tracked_entry(fake_toggl, fake_store):
    fake_store[THING] = 555
    toggl_api.handler(_event(ev.DEVICE_WORK_COMPLETED), None)
    fake_toggl.stop_entry.assert_called_once_with(
        "tok", workspace_id=11, entry_id=555
    )
    assert THING not in fake_store   # cleared


def test_work_paused_stops_tracked_entry(fake_toggl, fake_store):
    fake_store[THING] = 333
    toggl_api.handler(_event(ev.DEVICE_WORK_PAUSED), None)
    fake_toggl.stop_entry.assert_called_once_with(
        "tok", workspace_id=11, entry_id=333
    )


def test_stop_with_no_tracked_uses_current_from_toggl(fake_toggl, fake_store):
    fake_toggl.current_entry.return_value = {"id": 4242}
    toggl_api.handler(_event(ev.DEVICE_WORK_COMPLETED), None)
    fake_toggl.stop_entry.assert_called_once_with(
        "tok", workspace_id=11, entry_id=4242
    )


def test_break_started_stops_entry(fake_toggl, fake_store):
    fake_store[THING] = 100
    toggl_api.handler(_event(ev.DEVICE_BREAK_STARTED), None)
    fake_toggl.stop_entry.assert_called_once()


def test_break_completed_is_noop(fake_toggl, fake_store):
    resp = toggl_api.handler(_event(ev.DEVICE_BREAK_COMPLETED), None)
    assert resp.get("skipped") == ev.DEVICE_BREAK_COMPLETED
    fake_toggl.start_entry.assert_not_called()
    fake_toggl.stop_entry.assert_not_called()


def test_work_started_pushes_task_name_to_shadow(fake_toggl, fake_store, fake_iot):
    """The whole point of this server-side push: device LCD shouldn't have
    to wait 5–10 s for the Toggl webhook to round-trip back. After
    start_entry, we resolve project name + colour and write desired
    straight to the shadow."""
    fake_toggl.get_project.return_value = {
        "name": "Learning Networking", "color": "#0b83d9",
    }
    toggl_api.handler(_event(ev.DEVICE_WORK_STARTED), None)

    fake_toggl.get_project.assert_called_once()
    assert len(fake_iot.pushed) == 1
    thing, payload = fake_iot.pushed[0]
    assert thing == THING
    desired = payload["state"]["desired"]
    assert desired["task_name"] == "Learning Networking"
    assert desired["project_color"] == "#0b83d9"


def test_adopt_existing_entry_also_pushes_task_name(fake_toggl, fake_store, fake_iot):
    """Loop guard adopts a running entry instead of starting; we still
    need to push the project name down so the LCD label is right."""
    fake_toggl.current_entry.return_value = {"id": 7777, "project_id": 555}
    fake_toggl.get_project.return_value = {"name": "Adopted", "color": None}
    toggl_api.handler(_event(ev.DEVICE_WORK_STARTED), None)

    fake_toggl.start_entry.assert_not_called()
    assert len(fake_iot.pushed) == 1
    assert fake_iot.pushed[0][1]["state"]["desired"]["task_name"] == "Adopted"


def test_no_project_id_skips_shadow_push(fake_toggl, fake_store, fake_iot, monkeypatch):
    """If there's neither a default nor a stored project, we don't push —
    a project_color or task_name we made up would be worse than no update."""
    # Reconfigure: no default project in the secret.
    monkeypatch.setattr(
        toggl_api.sec, "get_secret",
        lambda _: {"toggl": {"api_token": "t", "workspace_id": 11}},
    )
    toggl_api.handler(_event(ev.DEVICE_WORK_STARTED), None)
    fake_toggl.get_project.assert_not_called()
    assert fake_iot.pushed == []
