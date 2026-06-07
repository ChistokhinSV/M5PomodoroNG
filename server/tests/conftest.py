"""Common pytest fixtures.

The Lambda runtime mounts each function's CodeUri flat at /var/task/
(handler.py + sibling .py files), and the shared layer at /opt/python/.
Tests need to stage the same import paths so:
    from shared import ...   (layer)
    import toggl_client      (sibling of handler.py)
both resolve the way they will in production."""

from __future__ import annotations
import os
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]

# Handlers read env at module-import time (matching Lambda's import-once
# init phase). Seed defaults before any consumer module gets imported by a
# test file — the per-test `_set_env` fixture below still wins for any
# fixture-driven mutation pytest needs.
for _k, _v in (
    ("AWS_REGION",              "eu-central-1"),
    ("EVENT_BUS_NAME",          "m5pomodoro-events-test"),
    ("STATE_TABLE_NAME",        "m5pomodoro-state-test"),
    ("TARGET_THING_NAME",       "M5StackCoreTest"),
    ("CREDENTIALS_SECRET_ARN",  "arn:test:credentials"),
    ("CALENDAR_ID",             "primary"),
):
    os.environ.setdefault(_k, _v)

# /opt/python equivalent so `from shared import ...` works
sys.path.insert(0, str(REPO_ROOT / "shared" / "python"))

# repo root so tests can still do `from consumers.X import handler` to load
# the module; Python's import machinery treats consumers/ as a namespace
# package because it has no __init__.py at the level above handler.py.
sys.path.insert(0, str(REPO_ROOT))

# Each consumer/source directory is also added so its handler's sibling
# imports (e.g. `import toggl_client`) resolve at import time.
for _func_dir in [
    REPO_ROOT / "consumers" / "toggl_api",
    REPO_ROOT / "consumers" / "gcal_api",
    REPO_ROOT / "consumers" / "device_shadow",
    REPO_ROOT / "consumers" / "task_context",
    REPO_ROOT / "consumers" / "wake_resync",
    REPO_ROOT / "sources"   / "shadow_relay",
    REPO_ROOT / "sources"   / "toggl_webhook",
]:
    sys.path.insert(0, str(_func_dir))


@pytest.fixture(autouse=True)
def _set_env(monkeypatch):
    """Lambdas read several names from env; tests get safe defaults."""
    monkeypatch.setenv("AWS_REGION", "eu-central-1")
    monkeypatch.setenv("EVENT_BUS_NAME", "m5pomodoro-events-test")
    monkeypatch.setenv("STATE_TABLE_NAME", "m5pomodoro-state-test")
    monkeypatch.setenv("TARGET_THING_NAME", "M5StackCoreTest")
    monkeypatch.setenv("CREDENTIALS_SECRET_ARN", "arn:test:credentials")
    monkeypatch.setenv("CALENDAR_ID", "primary")
