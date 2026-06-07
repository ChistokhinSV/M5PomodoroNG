"""Thin Google Calendar v3 client wrapper.

Uses a service-account JSON (downloaded from GCP Console). The calendar
that we write to must be shared with the service-account's email address
(GCP Console -> the SA -> Email; in Calendar -> Settings -> Share with
specific people, add that email with "Make changes to events").

Client is cached at module level so warm invocations skip the auth setup.
"""

from __future__ import annotations
import json
import logging
from typing import Optional

from google.oauth2 import service_account
from googleapiclient.discovery import build

log = logging.getLogger(__name__)

SCOPES = ["https://www.googleapis.com/auth/calendar"]

_service = None
# Cached colors().get() response: dict of color_id -> (r, g, b) for the
# 11 GCal event colors. Same value for every invocation of this container.
_event_colors_rgb: Optional[dict[str, tuple[int, int, int]]] = None


def _get_service(service_account_info: dict):
    """Build and cache the calendar service client."""
    global _service
    if _service is not None:
        return _service
    creds = service_account.Credentials.from_service_account_info(
        service_account_info, scopes=SCOPES
    )
    _service = build("calendar", "v3", credentials=creds, cache_discovery=False)
    return _service


def _hex_to_rgb(hex_color: str) -> Optional[tuple[int, int, int]]:
    """Accepts "#rrggbb" or "rrggbb". Returns None on malformed input."""
    if not hex_color:
        return None
    s = hex_color.lstrip("#").strip()
    if len(s) != 6:
        return None
    try:
        return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))
    except ValueError:
        return None


def _event_colors(service_account_info: dict) -> dict[str, tuple[int, int, int]]:
    """Fetch and cache the 11 GCal event-color backgrounds as RGB tuples."""
    global _event_colors_rgb
    if _event_colors_rgb is not None:
        return _event_colors_rgb
    service = _get_service(service_account_info)
    resp = service.colors().get().execute()
    out: dict[str, tuple[int, int, int]] = {}
    for color_id, definition in (resp.get("event") or {}).items():
        rgb = _hex_to_rgb(definition.get("background", ""))
        if rgb:
            out[color_id] = rgb
    _event_colors_rgb = out
    return out


def closest_color_id(service_account_info: dict,
                     hex_color: str) -> Optional[str]:
    """Map an arbitrary "#rrggbb" colour to the closest of GCal's 11 event
    color ids (Euclidean distance in RGB). Returns None if the input is
    unparseable or the colors API call fails — caller should drop the
    colorId and let GCal use the calendar default.
    """
    target = _hex_to_rgb(hex_color)
    if target is None:
        return None
    try:
        palette = _event_colors(service_account_info)
    except Exception as e:                          # noqa: BLE001
        log.warning("colors() lookup failed: %s", e)
        return None
    if not palette:
        return None

    best_id: Optional[str] = None
    best_dist = float("inf")
    tr, tg, tb = target
    for color_id, (r, g, b) in palette.items():
        d = (r - tr) ** 2 + (g - tg) ** 2 + (b - tb) ** 2
        if d < best_dist:
            best_dist = d
            best_id = color_id
    return best_id


def insert_event(
    service_account_info: dict,
    *,
    calendar_id: str,
    summary: str,
    description: str,
    start_iso: str,
    end_iso: str,
    extended_properties: Optional[dict] = None,
    color_id: Optional[str] = None,
) -> dict:
    """Create a single calendar event. Times are ISO-8601 with timezone offset.

    The `extended_properties.private` map is the documented way to attach
    machine-readable metadata that survives round-trips and is searchable
    via .list(privateExtendedProperty=...). We use it for the dedup id.
    """
    body = {
        "summary":     summary,
        "description": description,
        "start":       {"dateTime": start_iso},
        "end":         {"dateTime": end_iso},
    }
    if extended_properties:
        body["extendedProperties"] = {"private": extended_properties}
    if color_id:
        body["colorId"] = color_id

    service = _get_service(service_account_info)
    return service.events().insert(
        calendarId=calendar_id, body=body
    ).execute()


def find_event_by_extended_property(
    service_account_info: dict,
    *,
    calendar_id: str,
    key: str,
    value: str,
) -> Optional[dict]:
    """Look up an event by a private extended property — used for GCal-side
    dedup checks before insert."""
    service = _get_service(service_account_info)
    resp = service.events().list(
        calendarId=calendar_id,
        privateExtendedProperty=f"{key}={value}",
        maxResults=1,
    ).execute()
    items = resp.get("items") or []
    return items[0] if items else None
