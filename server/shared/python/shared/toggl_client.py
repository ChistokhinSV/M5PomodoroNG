"""Thin Toggl Track v9 client. stdlib only — packaged into the shared layer
so both consumer-toggl-api and consumer-wake-resync can use it without
duplicating the API surface.

Auth: HTTP Basic with the API token as username and the literal string
"api_token" as the password (Toggl convention).

Only the calls this app needs are implemented; extend as needed.
"""

from __future__ import annotations
import base64
import json
import logging
import urllib.error
import urllib.request
from datetime import datetime, timezone
from typing import Optional

log = logging.getLogger(__name__)

BASE = "https://api.track.toggl.com/api/v9"
DEVICE_TAG = "m5pomodoro-device"


def _auth_header(api_token: str) -> dict[str, str]:
    raw = f"{api_token}:api_token".encode("utf-8")
    return {
        "Authorization": "Basic " + base64.b64encode(raw).decode("ascii"),
        "User-Agent":    "m5pomodoro-bridge",
        "Content-Type":  "application/json",
    }


def _request(
    method: str, url: str, *,
    headers: dict[str, str],
    body: Optional[dict] = None,
    timeout: float = 8.0,
) -> dict:
    data = None
    if body is not None:
        data = json.dumps(body).encode("utf-8")
    req = urllib.request.Request(url, method=method, headers=headers, data=data)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read()
            # Toggl returns 200 + null for "no running entry"; treat as {}.
            if not raw or raw == b"null":
                return {}
            return json.loads(raw)
    except urllib.error.HTTPError as e:
        # Stop on an already-stopped entry returns 409; surface for caller.
        if e.code == 409:
            return {"_status": 409}
        body_preview = e.read()[:200].decode("utf-8", errors="replace")
        log.error("Toggl HTTP %d %s -> %s",
                  e.code, url, body_preview)
        raise


def current_entry(api_token: str, *, timeout: float = 8.0) -> Optional[dict]:
    """Return the user's currently-running time entry, or None."""
    data = _request("GET", f"{BASE}/me/time_entries/current",
                    headers=_auth_header(api_token), timeout=timeout)
    return data or None


def start_entry(
    api_token: str,
    *,
    workspace_id: int,
    project_id: Optional[int] = None,
    description: Optional[str] = None,
    timeout: float = 8.0,
) -> dict:
    """Start a new time entry tagged with the device marker."""
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    body = {
        "created_with": "m5pomodoro-bridge",
        "start": now,
        "duration": -1,                 # v9 convention for "running"
        "workspace_id": workspace_id,
        "tags": [DEVICE_TAG],
    }
    if project_id:
        body["project_id"] = project_id
    if description:
        body["description"] = description

    return _request(
        "POST", f"{BASE}/workspaces/{workspace_id}/time_entries",
        headers=_auth_header(api_token), body=body, timeout=timeout,
    )


def stop_entry(
    api_token: str,
    *,
    workspace_id: int,
    entry_id: int,
    timeout: float = 8.0,
) -> dict:
    """Stop a running time entry. Stopping an already-stopped entry returns
    409, which we treat as success (`already_stopped`)."""
    resp = _request(
        "PATCH",
        f"{BASE}/workspaces/{workspace_id}/time_entries/{entry_id}/stop",
        headers=_auth_header(api_token), timeout=timeout,
    )
    if resp.get("_status") == 409:
        return {"id": entry_id, "already_stopped": True}
    return resp


def get_project(
    api_token: str,
    *,
    workspace_id: int,
    project_id: int,
    timeout: float = 4.0,
) -> Optional[dict]:
    """Look up a project record; returns None on failure (e.g. project
    deleted, permission, network blip). Used to resolve names for display."""
    try:
        return _request(
            "GET", f"{BASE}/workspaces/{workspace_id}/projects/{project_id}",
            headers=_auth_header(api_token), timeout=timeout,
        )
    except urllib.error.URLError as e:
        log.warning("Toggl get_project %s/%s failed: %s",
                    workspace_id, project_id, e)
        return None
