"""Event vocabulary for the m5pomodoro EventBridge bus.

Centralizes the `detail-type` strings every source and consumer agrees on.
Two namespaces so consumers can't feedback-loop:

  device.session.*   - emitted by source-shadow-relay (firmware shadow updates)
  external.<svc>.*   - emitted by source-<svc>-webhook (third-party webhooks)

Payload schemas are plain stdlib dataclasses — no pydantic — so the layer
ships with no requirements.txt and SAM can zip it as-is on Windows without
needing `make` or `pip install`. Validation at consumer boundaries is left
to the handlers (they read `.get(...)` rather than `.attr`); the dataclass
shape is documentation-by-code for what each event carries.
"""

from __future__ import annotations
import dataclasses
import json
from dataclasses import dataclass, field
from typing import Optional

# ---------------------------------------------------------------------------
# detail-type constants
# ---------------------------------------------------------------------------

DEVICE_WORK_STARTED    = "device.session.work.started"
DEVICE_WORK_PAUSED     = "device.session.work.paused"
DEVICE_WORK_RESUMED    = "device.session.work.resumed"
DEVICE_WORK_COMPLETED  = "device.session.work.completed"
DEVICE_BREAK_STARTED   = "device.session.break.started"
DEVICE_BREAK_COMPLETED = "device.session.break.completed"
DEVICE_CYCLE_COMPLETED = "device.session.cycle.completed"
DEVICE_SESSION_PREFIX  = "device.session."

# Fired by shadow-relay when reported.wake_id changes — that's the device's
# signal that it just booted or woke from sleep. Carries the entire reported
# state in detail so the consumer can read duration_min / device_state /
# task_name without re-fetching the shadow.
DEVICE_WAKE            = "device.wake"

EXTERNAL_TOGGL_STARTED = "external.toggl.timer.started"
EXTERNAL_TOGGL_STOPPED = "external.toggl.timer.stopped"
EXTERNAL_TOGGL_PREFIX  = "external.toggl."

# `source` field for PutEventsEntry; mostly for observability since routing
# is driven off detail-type.
SOURCE_SHADOW         = "m5pomodoro.shadow"
SOURCE_TOGGL_WEBHOOK  = "m5pomodoro.webhook.toggl"

DEFAULT_BUS = "m5pomodoro-events"


# ---------------------------------------------------------------------------
# Payload models — plain dataclasses. The `detail` portion of a bus event.
# ---------------------------------------------------------------------------

@dataclass
class DeviceSessionDetail:
    """Payload for every `device.session.*` event."""
    thing_name: str
    timestamp: int
    event_id: str
    session_type: Optional[str] = None       # "work" | "short_break" | "long_break"
    session_number: Optional[int] = None
    total_sessions: Optional[int] = None
    duration_min: Optional[int] = None
    today: Optional[int] = None
    week: Optional[int] = None
    lifetime: Optional[int] = None
    # Project label echoed in the shadow when a webhook source set it.
    # gcal_api appends it to the calendar event summary.
    task_name: Optional[str] = None
    # How the device transitioned — "device" (button/gyro/timeout) or
    # "shadow_command" (a shadow delta drove this). Lets toggl-api tell apart
    # a user pressing unpause (which should restart Toggl) from the cloud
    # issuing a resume verb (which must not, or we loop with whatever
    # restarted Toggl on the third-party side). Default attribution is
    # "device" when the field is absent from the shadow snapshot — that's
    # the safe legacy interpretation matching pre-feature firmware.
    state_change_source: Optional[str] = None
    # AWS-side shadow version of the snapshot this event was derived from.
    # The same number appears in the device's own delta logs (`v=N`), so
    # device-log entries can be matched 1:1 with cloud CloudWatch entries.
    shadow_version: Optional[int] = None


@dataclass
class DeviceWakeDetail:
    """Payload for `device.wake` events. Carries the device's full reported
    state at the moment of the wake_id change so the wake-resync consumer
    can decide what to do without an extra get_thing_shadow call."""
    thing_name: str
    timestamp: int
    event_id: str
    wake_id: int
    device_state: Optional[str] = None        # "idle"/"active"/"paused"
    session_type: Optional[str] = None
    session_number: Optional[int] = None
    total_sessions: Optional[int] = None
    duration_min: Optional[int] = None        # device's configured work duration
    task_name: Optional[str] = None
    today: Optional[int] = None
    week: Optional[int] = None
    lifetime: Optional[int] = None


@dataclass
class ExternalTogglDetail:
    """Payload for every `external.toggl.*` event.

    project_name and project_color are resolved by the webhook source at
    receive time (one extra Toggl API call) so downstream consumers don't
    need Toggl creds just to display "Project X" on the device or tint a
    GCal entry to match.
    """
    thing_name: str
    timestamp: int
    event_id: str
    toggl_entry_id: int
    toggl_project_id: Optional[int] = None
    project_name: Optional[str] = None
    project_color: Optional[str] = None      # "#RRGGBB" — Toggl's project color
    description: Optional[str] = None
    tags: list[str] = field(default_factory=list)


def _to_json(obj) -> str:
    """asdict + filter Nones, then json.dumps. Matches pydantic's
    `model_dump_json(exclude_none=True)` semantics for our payloads."""
    data = {k: v for k, v in dataclasses.asdict(obj).items() if v is not None}
    return json.dumps(data, separators=(",", ":"))


# ---------------------------------------------------------------------------
# Builders that produce the dict shape boto3 events client wants.
# ---------------------------------------------------------------------------

def device_event(detail_type: str, detail: DeviceSessionDetail, *,
                 bus: str = DEFAULT_BUS) -> dict:
    """Build a PutEventsEntry for a device.session.* event."""
    assert detail_type.startswith(DEVICE_SESSION_PREFIX), \
        f"detail_type {detail_type!r} not in device.session.* namespace"
    return {
        "Source":       SOURCE_SHADOW,
        "DetailType":   detail_type,
        "Detail":       _to_json(detail),
        "EventBusName": bus,
    }


def device_wake_event(detail: DeviceWakeDetail, *,
                      bus: str = DEFAULT_BUS) -> dict:
    """PutEventsEntry for a device.wake event."""
    return {
        "Source":       SOURCE_SHADOW,
        "DetailType":   DEVICE_WAKE,
        "Detail":       _to_json(detail),
        "EventBusName": bus,
    }


def external_toggl_event(detail_type: str, detail: ExternalTogglDetail, *,
                         bus: str = DEFAULT_BUS) -> dict:
    """Build a PutEventsEntry for an external.toggl.* event."""
    assert detail_type.startswith(EXTERNAL_TOGGL_PREFIX), \
        f"detail_type {detail_type!r} not in external.toggl.* namespace"
    return {
        "Source":       SOURCE_TOGGL_WEBHOOK,
        "DetailType":   detail_type,
        "Detail":       _to_json(detail),
        "EventBusName": bus,
    }


def synth_event_id(thing_name: str, timestamp: int, detail_type: str) -> str:
    """Deterministic id from the natural transition keys. Replays of the
    same (device, time, transition) tuple produce the same id, which is
    what consumers' idempotency keys hash on."""
    return f"{thing_name}:{timestamp}:{detail_type}"
