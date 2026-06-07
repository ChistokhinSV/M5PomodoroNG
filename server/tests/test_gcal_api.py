"""Insert + GCal-side dedup for consumer-gcal-api."""

from __future__ import annotations
from unittest.mock import MagicMock

import pytest

from shared import events as ev
from consumers.gcal_api import handler as gcal_api


THING = "M5StackCore2"


@pytest.fixture
def fake_gcal(monkeypatch):
    """Stateful mock for the GCal client. insert_event remembers each
    event_id; find_event_by_extended_property returns it on later calls.
    This is exactly the dedup invariant we rely on in production."""
    inserted_by_event_id: dict[str, dict] = {}

    fake = MagicMock()

    def fake_insert(sa, *, calendar_id, summary, description,
                    start_iso, end_iso, extended_properties,
                    color_id=None):
        eid = (extended_properties or {}).get("event_id")
        created = {
            "id": f"gcal-{eid}",
            "summary": summary,
            "extendedProperties": {"private": extended_properties},
        }
        if color_id:
            created["colorId"] = color_id
        if eid:
            inserted_by_event_id[eid] = created
        return created

    def fake_find(sa, *, calendar_id, key, value):
        if key == "event_id" and value in inserted_by_event_id:
            return inserted_by_event_id[value]
        return None

    # closest_color_id returns None unless a test overrides it — keeps the
    # colorId path inert for legacy tests that don't care about colour.
    fake.closest_color_id.return_value = None

    fake.insert_event.side_effect = fake_insert
    fake.find_event_by_extended_property.side_effect = fake_find
    monkeypatch.setattr(gcal_api, "gcal_client", fake)
    return fake


@pytest.fixture(autouse=True)
def _patch_secrets(monkeypatch):
    monkeypatch.setattr(
        gcal_api.sec, "get_secret",
        lambda _: {"client_email": "svc@proj.iam.gserviceaccount.com"},
    )


@pytest.fixture(autouse=True)
def _patch_state_store(monkeypatch):
    """Without this every test would try to GetItem against a real DDB
    table for the task_context-driven colour lookup. Tests that exercise
    the colour path override the patch themselves."""
    monkeypatch.setattr(
        gcal_api.state_store, "get_task_context", lambda _thing: None,
    )


def _event(detail_type, event_id="ev-1", **detail):
    base = {"thing_name": THING, "event_id": event_id,
            "timestamp": 1700001500, "duration_min": 25,
            "session_number": 2, "total_sessions": 4,
            "today": 3, "week": 12, "lifetime": 100}
    base.update(detail)
    return {"detail-type": detail_type, "detail": base}


def test_work_completed_inserts_event(fake_gcal):
    resp = gcal_api.handler(_event(ev.DEVICE_WORK_COMPLETED), None)
    assert resp["ok"]
    fake_gcal.insert_event.assert_called_once()
    kwargs = fake_gcal.insert_event.call_args.kwargs
    assert "Pomodoro: work 2/4" in kwargs["summary"]
    assert kwargs["extended_properties"]["event_id"] == "ev-1"


def test_replay_dedups_via_gcal_lookup(fake_gcal):
    """Replays of the same event_id only insert once. The second
    invocation finds the existing GCal event by its extendedProperty
    and short-circuits — no DDB marker is needed for correctness."""
    e = _event(ev.DEVICE_WORK_COMPLETED, event_id="dup")
    gcal_api.handler(e, None)
    gcal_api.handler(e, None)
    fake_gcal.insert_event.assert_called_once()


def test_pre_existing_gcal_event_skips_insert(fake_gcal):
    """If the event already exists in GCal (e.g. a previous run inserted
    it and we never got around to writing the local marker), we treat it
    as a no-op."""
    # Pre-seed the lookup result by going through one insert first.
    fake_gcal.find_event_by_extended_property.side_effect = lambda sa, **k: \
        {"id": "old"} if k.get("value") == "x" else None

    resp = gcal_api.handler(_event(ev.DEVICE_WORK_COMPLETED, event_id="x"), None)
    assert resp["skipped"] == "exists_in_gcal"
    fake_gcal.insert_event.assert_not_called()


def test_cycle_completed_summary(fake_gcal):
    gcal_api.handler(
        _event(ev.DEVICE_CYCLE_COMPLETED, event_id="cyc-1", today=7), None
    )
    summary = fake_gcal.insert_event.call_args.kwargs["summary"]
    assert "cycle complete" in summary.lower()
    assert "7" in summary


def test_time_window_uses_duration(fake_gcal):
    gcal_api.handler(_event(ev.DEVICE_WORK_COMPLETED, event_id="t"), None)
    kw = fake_gcal.insert_event.call_args.kwargs
    # 1700001500 - 25*60 = 1700000000; iso strings will end the same minute apart.
    assert kw["start_iso"].startswith("2023-11-14T")
    assert kw["end_iso"].startswith("2023-11-14T")


def test_task_name_appended_to_summary(fake_gcal):
    """Shadow-relay forwards reported.task_name into DeviceSessionDetail;
    gcal entry should mention the project so a calendar week-view is
    actually useful for figuring out what got worked on."""
    gcal_api.handler(
        _event(ev.DEVICE_WORK_COMPLETED, event_id="tn",
               task_name="Learning Networking"),
        None,
    )
    kw = fake_gcal.insert_event.call_args.kwargs
    assert "Pomodoro: work 2/4" in kw["summary"]
    assert "Learning Networking" in kw["summary"]
    assert "task: Learning Networking" in kw["description"]


def test_color_resolved_from_task_context(fake_gcal, monkeypatch):
    """When the toggl webhook stored a project_color in the task_context
    record, gcal_api should map it to a GCal colorId via the colors API
    and pass it on the insert."""
    monkeypatch.setattr(
        gcal_api.state_store, "get_task_context",
        lambda _thing: {
            "task_name": "Learning Networking",
            "provider": "toggl",
            "provider_ref": {"project_color": "#0b83d9"},
        },
    )
    fake_gcal.closest_color_id.return_value = "9"

    resp = gcal_api.handler(
        _event(ev.DEVICE_WORK_COMPLETED, event_id="clr"), None,
    )
    assert resp["color_id"] == "9"
    fake_gcal.closest_color_id.assert_called_once()
    # ensure the colour hex flows untouched into closest_color_id
    assert fake_gcal.closest_color_id.call_args.args[1] == "#0b83d9"
    assert fake_gcal.insert_event.call_args.kwargs["color_id"] == "9"


def test_missing_task_context_does_not_block_insert(fake_gcal, monkeypatch):
    """task_context lookup blowing up (DDB outage etc) must not stop the
    calendar event from landing — colour is a nice-to-have, the event
    isn't."""
    def explode(_thing):
        raise RuntimeError("ddb down")
    monkeypatch.setattr(gcal_api.state_store, "get_task_context", explode)

    resp = gcal_api.handler(
        _event(ev.DEVICE_WORK_COMPLETED, event_id="resilient"), None,
    )
    assert resp["ok"]
    assert resp["color_id"] is None
    fake_gcal.insert_event.assert_called_once()
