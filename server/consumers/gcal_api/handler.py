"""consumer-gcal-api Lambda.

Trigger: EventBridge rule matching
  detail-type in {device.session.work.completed, device.session.cycle.completed}.

Builds a Google Calendar event for each completed work / cycle and inserts
it on the configured calendar.

Idempotency: the GCal event carries `extendedProperties.private.event_id`,
which is the firmware-synthesised id from (thing_name, last_event_at,
detail_type). On replay we look up by that key first; if the event already
exists we return success without re-inserting. No DDB marker is used —
relying on a marker in a system we don't write to (DDB) when the actual
side effect lives in another system (GCal) is fragile: if a Lambda fails
mid-call we'd end up with a marker that prevents future retries from ever
landing the calendar event.
"""

from __future__ import annotations
import json
import logging
import os
from datetime import datetime, timezone

from shared import events as ev
from shared import secrets as sec
from shared import state_store

# Sibling import — both files land in /var/task/ once Lambda unpacks CodeUri.
import gcal_client

log = logging.getLogger()
log.setLevel(logging.INFO)

CREDENTIALS_SECRET_ARN = os.environ["CREDENTIALS_SECRET_ARN"]
CALENDAR_ID            = os.environ["CALENDAR_ID"]


def _service_account_info() -> dict:
    return sec.get_secret(CREDENTIALS_SECRET_ARN)["gcal_service_account"]


def _iso(ts: int) -> str:
    return datetime.fromtimestamp(ts, tz=timezone.utc).isoformat()


def _build_event(detail_type: str, detail: dict) -> dict:
    """Compute summary, description, and time window from the event detail."""
    now           = int(detail.get("timestamp", 0))
    duration_min  = int(detail.get("duration_min") or 25)
    session_num   = detail.get("session_number")
    total         = detail.get("total_sessions")
    today         = detail.get("today")
    week          = detail.get("week")
    lifetime      = detail.get("lifetime")
    thing_name    = detail.get("thing_name")
    task_name     = (detail.get("task_name") or "").strip() or None

    end_ts   = now
    start_ts = max(0, end_ts - duration_min * 60)

    if detail_type == ev.DEVICE_CYCLE_COMPLETED:
        base = (f"Pomodoro cycle complete ({today} today)"
                if today is not None else "Pomodoro cycle complete")
    else:
        base = (f"Pomodoro: work {session_num}/{total}"
                if session_num and total else "Pomodoro: work")

    # Project name as a suffix keeps the "Pomodoro:" prefix scannable in week
    # view and lets sort-by-summary still group Pomodoros together.
    summary = f"{base} — {task_name}" if task_name else base

    description_lines = [f"device: {thing_name}"]
    if task_name is not None: description_lines.append(f"task: {task_name}")
    if today    is not None: description_lines.append(f"today: {today}")
    if week     is not None: description_lines.append(f"week: {week}")
    if lifetime is not None: description_lines.append(f"lifetime: {lifetime}")
    description = "\n".join(description_lines)

    return {
        "summary": summary,
        "description": description,
        "start_iso": _iso(start_ts),
        "end_iso":   _iso(end_ts),
    }


def handler(event: dict, context) -> dict:
    detail_type = event.get("detail-type") or event.get("DetailType")
    detail = event.get("detail") or {}
    thing_name = detail.get("thing_name")
    event_id   = detail.get("event_id")

    log.info("gcal_api detail-type=%s thing=%s detail=%s",
             detail_type, thing_name, json.dumps(detail)[:200])

    if not detail_type or not thing_name or not event_id:
        log.warning("Missing detail-type / thing_name / event_id; ignoring")
        return {"ok": False, "reason": "malformed"}

    sa_info = _service_account_info()

    # Idempotency: ask GCal whether we already inserted this event_id.
    # Cheap GET — the only authoritative dedup signal.
    existing = gcal_client.find_event_by_extended_property(
        sa_info, calendar_id=CALENDAR_ID, key="event_id", value=event_id
    )
    if existing:
        log.info("GCal already has event for event_id=%s (id=%s); no-op",
                 event_id, existing.get("id"))
        return {"ok": True, "skipped": "exists_in_gcal"}

    # Pull project color from the latest task_context the webhook source
    # wrote to DDB. Falling back to None means GCal uses the calendar's
    # default event color — no hard failure when the device started a
    # session without a Toggl entry being active.
    color_id = _resolve_color_id(sa_info, thing_name)

    fields = _build_event(detail_type, detail)
    created = gcal_client.insert_event(
        sa_info,
        calendar_id=CALENDAR_ID,
        extended_properties={"event_id": event_id, "thing_name": thing_name},
        color_id=color_id,
        **fields,
    )
    log.info("Inserted GCal event id=%s for %s (color_id=%s)",
             created.get("id"), event_id, color_id)
    return {"ok": True, "gcal_event_id": created.get("id"),
            "color_id": color_id}


def _resolve_color_id(sa_info: dict, thing_name: str) -> str | None:
    """Map the per-device project_color (set by task_context on the latest
    timer-start webhook) to one of GCal's 11 event color ids.

    Soft-fails to None on any error — colour is a nice-to-have, missing it
    must never block a calendar entry from being created."""
    try:
        ctx = state_store.get_task_context(thing_name)
    except Exception as e:                              # noqa: BLE001
        log.warning("task_context lookup failed for %s: %s", thing_name, e)
        return None
    if not ctx:
        return None
    project_color = (ctx.get("provider_ref") or {}).get("project_color")
    if not project_color:
        return None
    return gcal_client.closest_color_id(sa_info, project_color)
