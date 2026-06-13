"""Lightweight correlation-id logging adapter.

Every Lambda invocation gets:
  - aws_request_id  (Lambda's own per-invocation id; in CloudWatch already)
  - event_id        (synthesised by source-shadow-relay / source-*-webhook;
                     same id appears on the device side via command_id)
  - shadow_version  (the post-update AWS shadow `version` — also echoed in
                     the firmware's "[Shadow] Delta v=N ..." log lines, so
                     a single number ties a device line to a cloud line)

`bind(...)` returns a logging.LoggerAdapter that prepends
`req=<aws_req> ev=<event_id> v=<shadow_version>` to every log message.
Handlers call it once at the top of `handler(...)` and use the returned
logger everywhere instead of the module-level `log`.

This module is intentionally tiny (no requirements.txt entries) so it ships
inside the existing shared Lambda layer with zero packaging churn.
"""

from __future__ import annotations
import logging
from typing import Any, Optional


def _short(value: Optional[str], limit: int = 12) -> str:
    """Trim long request ids so log lines stay readable. CloudWatch keeps
    the full id elsewhere (the END/REPORT lines), so the short prefix is
    enough for visual scanning + grep."""
    if not value:
        return "-"
    s = str(value)
    return s if len(s) <= limit else s[:limit]


def bind(
    base_logger: logging.Logger,
    *,
    aws_request_id: Optional[str] = None,
    event_id: Optional[str] = None,
    shadow_version: Optional[Any] = None,
    extra: Optional[dict] = None,
) -> logging.LoggerAdapter:
    """Return a LoggerAdapter that stamps correlation ids on every message.

    `extra` is an open-ended dict — handlers can stuff anything additional
    they want to surface ('thing=...', 'detail-type=...', etc.) without
    needing a new explicit kwarg per call site.
    """
    parts: list[str] = []
    parts.append(f"req={_short(aws_request_id, 12)}")
    if event_id:
        parts.append(f"ev={_short(event_id, 56)}")
    if shadow_version is not None:
        parts.append(f"v={shadow_version}")
    if extra:
        for k, v in extra.items():
            if v is None:
                continue
            parts.append(f"{k}={v}")
    prefix = " ".join(parts)

    class _PrefixAdapter(logging.LoggerAdapter):
        def process(self, msg, kwargs):
            return f"[{prefix}] {msg}", kwargs

    return _PrefixAdapter(base_logger, {})


def request_id(context) -> str:
    """Pull the Lambda invocation id from the context object, defensively.
    Local-test invocations pass `None` for context — return a sentinel
    instead of crashing."""
    if context is None:
        return "local"
    return getattr(context, "aws_request_id", None) or "unknown"
