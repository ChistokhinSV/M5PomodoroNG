"""HMAC validation + classification for source-toggl-webhook."""

from __future__ import annotations
import hashlib
import hmac
import json
from unittest.mock import patch, MagicMock

import pytest

# Patch boto3 before importing the handler -- the module instantiates an
# events client at import time.
with patch("boto3.client") as _mock_boto:
    _mock_boto.return_value = MagicMock()
    from sources.toggl_webhook import handler as toggl_webhook


SIGNING_SECRET = "test-signing-secret"


def _signed(body: dict) -> dict:
    raw = json.dumps(body)
    sig = hmac.new(
        SIGNING_SECRET.encode(), raw.encode(), hashlib.sha256
    ).hexdigest()
    return {
        "body": raw,
        "headers": {"x-webhook-signature-256": f"sha256={sig}"},
        "isBase64Encoded": False,
    }


@pytest.fixture(autouse=True)
def _patch_secrets(monkeypatch):
    monkeypatch.setattr(
        toggl_webhook.sec, "get_secret",
        lambda _: {"signing_secret": SIGNING_SECRET},
    )


@pytest.fixture(autouse=True)
def _mock_put_events(monkeypatch):
    mock = MagicMock(return_value={"FailedEntryCount": 0})
    monkeypatch.setattr(toggl_webhook._events, "put_events", mock)
    return mock


def test_bad_signature_rejected():
    body = {"foo": "bar"}
    bad_event = {
        "body": json.dumps(body),
        "headers": {"x-webhook-signature-256": "sha256=deadbeef"},
        "isBase64Encoded": False,
    }
    resp = toggl_webhook.handler(bad_event, None)
    assert resp["statusCode"] == 401


def test_validation_code_handshake():
    event = _signed({"validation_code": "abc123"})
    resp = toggl_webhook.handler(event, None)
    assert resp["statusCode"] == 200
    assert json.loads(resp["body"])["validation_code"] == "abc123"


def test_time_entry_created_running_emits_started(_mock_put_events):
    # Toggl v9 webhook shape: action+model in metadata, entry in payload.
    body = {
        "metadata": {"action": "created", "model": "time_entry"},
        "payload": {"id": 42, "duration": -1, "project_id": 9, "tags": ["focus"]},
    }
    resp = toggl_webhook.handler(_signed(body), None)
    assert resp["statusCode"] == 200
    args, kwargs = _mock_put_events.call_args
    entries = kwargs.get("Entries") or args[0]
    assert len(entries) == 1
    assert entries[0]["DetailType"] == "external.toggl.timer.started"


def test_time_entry_updated_stopped_emits_stopped(_mock_put_events):
    body = {
        "metadata": {"action": "updated", "model": "time_entry"},
        "payload": {"id": 42, "duration": 1500},  # positive = stopped
    }
    resp = toggl_webhook.handler(_signed(body), None)
    assert resp["statusCode"] == 200
    entries = _mock_put_events.call_args.kwargs["Entries"]
    assert entries[0]["DetailType"] == "external.toggl.timer.stopped"


def test_non_time_entry_event_ignored(_mock_put_events):
    body = {
        "metadata": {"action": "updated", "model": "project"},
        "payload": {"id": 1},
    }
    resp = toggl_webhook.handler(_signed(body), None)
    assert resp["statusCode"] == 200
    assert json.loads(resp["body"])["ignored"] is True
    _mock_put_events.assert_not_called()
