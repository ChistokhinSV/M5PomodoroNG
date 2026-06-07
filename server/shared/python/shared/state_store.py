"""DynamoDB-backed runtime state for the consumer Lambdas.

Single on-demand table, composite key (thing_name + sort key) so multiple
devices share the table without contention. Three flavors of record:

  PK=<thing>  SK="toggl#running"       attrs: entry_id, updated_at
  PK=<thing>  SK="task_context"        attrs: task_name, provider, provider_ref
  PK=<thing>  SK="processed#<eid>"     attrs: processed_at  (TTL ~30 days)

The processed-record TTL means we don't grow the table forever. Conditional
writes give us idempotent mark-as-processed: if two replays race, exactly
one wins.
"""

from __future__ import annotations
import os
import time
from typing import Optional

import boto3
from botocore.exceptions import ClientError

# 30-day retention for processed-event markers. Shorter than EventBridge's
# replay window so any actual replay we'd want still works; long enough that
# we don't accidentally double-process anything in normal operation.
PROCESSED_TTL_SECONDS = 30 * 24 * 60 * 60


def _table():
    region = os.environ.get("AWS_REGION", "eu-central-1")
    name = os.environ["STATE_TABLE_NAME"]
    return boto3.resource("dynamodb", region_name=region).Table(name)


# ---------- Toggl running-entry pointer ------------------------------------

def get_running_entry(thing_name: str) -> Optional[int]:
    """Returns the Toggl entry id we currently believe is running for this
    device, or None if we don't have one tracked. Used by consumer-toggl-api
    to know what to PUT a stop on."""
    resp = _table().get_item(Key={"PK": thing_name, "SK": "toggl#running"})
    item = resp.get("Item")
    return int(item["entry_id"]) if item else None


def set_running_entry(thing_name: str, entry_id: int) -> None:
    _table().put_item(Item={
        "PK": thing_name,
        "SK": "toggl#running",
        "entry_id": entry_id,
        "updated_at": int(time.time()),
    })


def clear_running_entry(thing_name: str) -> None:
    _table().delete_item(Key={"PK": thing_name, "SK": "toggl#running"})


# ---------- task context (current project per device) ---------------------

def set_task_context(
    thing_name: str,
    *,
    task_name: str,
    provider: str,
    provider_ref: dict,
) -> None:
    """Stash what we'd consider the "current task" for this device. Used by
    consumer-task-context to record the latest webhook-driven update (one
    write per timer-start) and by consumer-toggl-api to recall it when the
    device kicks off the next session.

    provider_ref is a JSON-friendly dict of provider-specific tokens
    (Toggl: project_id, workspace_id, description, tags). Keeping it
    opaque means adding a future provider doesn't change this table's
    shape -- only the consumers know what to do with their own provider's
    keys.
    """
    _table().put_item(Item={
        "PK": thing_name,
        "SK": "task_context",
        "task_name": task_name,
        "provider": provider,
        "provider_ref": provider_ref,
        "updated_at": int(time.time()),
    })


def get_task_context(thing_name: str) -> Optional[dict]:
    """Return the most-recent task context, or None if nothing's ever been
    seen for this device. Callers should handle the None and fall back to
    their provider's default config."""
    resp = _table().get_item(Key={"PK": thing_name, "SK": "task_context"})
    item = resp.get("Item")
    if not item:
        return None
    return {
        "task_name":    item.get("task_name"),
        "provider":     item.get("provider"),
        "provider_ref": item.get("provider_ref") or {},
        "updated_at":   item.get("updated_at"),
    }


# ---------- idempotency markers --------------------------------------------

def mark_processed(thing_name: str, event_id: str) -> bool:
    """Attempt to record (thing_name, event_id) as processed. Returns True if
    this caller "won" the race (first write); False if it was already there.

    Consumers should treat False as "another invocation already handled this
    event, do nothing further."
    """
    expires_at = int(time.time()) + PROCESSED_TTL_SECONDS
    try:
        _table().put_item(
            Item={
                "PK": thing_name,
                "SK": f"processed#{event_id}",
                "processed_at": int(time.time()),
                "ttl": expires_at,
            },
            # Only succeed if no item with this PK+SK already exists.
            ConditionExpression="attribute_not_exists(PK)",
        )
        return True
    except ClientError as e:
        if e.response["Error"]["Code"] == "ConditionalCheckFailedException":
            return False
        raise


def was_processed(thing_name: str, event_id: str) -> bool:
    """Read-only check. Pair with mark_processed for the typical
    "check then mark" idempotency pattern, OR rely on mark_processed's
    atomic conditional write alone (cheaper, one round-trip)."""
    resp = _table().get_item(
        Key={"PK": thing_name, "SK": f"processed#{event_id}"}
    )
    return "Item" in resp
