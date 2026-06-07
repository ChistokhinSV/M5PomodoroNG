"""AWS Secrets Manager helper with module-level cache.

Lambdas pay an SSM round-trip on cold start; the cache keeps every warm
invocation cheap. Cache is keyed by secret name and persists for the lifetime
of the Lambda container, which is what we want — Secrets Manager rotation
that flips the underlying secret without a Lambda redeploy is rare for this
class of app, and if needed the user can force-reset by toggling an env var.
"""

from __future__ import annotations
import json
import os
from typing import Any

import boto3

_cache: dict[str, Any] = {}


def _client():
    # Lazy-construct so import is cheap during tests that don't need AWS.
    region = os.environ.get("AWS_REGION", "eu-central-1")
    return boto3.client("secretsmanager", region_name=region)


def get_secret(name_or_arn: str) -> dict:
    """Return the JSON-parsed secret. Raises if not JSON or not found.

    Caller passes either the friendly name ("m5pomodoro/toggl/api") or the
    full ARN (what the SAM template will resolve at deploy time). Both work.
    """
    if name_or_arn in _cache:
        return _cache[name_or_arn]

    resp = _client().get_secret_value(SecretId=name_or_arn)
    raw = resp.get("SecretString")
    if raw is None:
        raise ValueError(f"Secret {name_or_arn!r} has no SecretString payload")
    try:
        parsed = json.loads(raw)
    except json.JSONDecodeError as e:
        raise ValueError(
            f"Secret {name_or_arn!r} is not valid JSON: {e}"
        ) from e

    _cache[name_or_arn] = parsed
    return parsed


def clear_cache() -> None:
    """Test hook — drop everything cached. Not used in prod."""
    _cache.clear()
