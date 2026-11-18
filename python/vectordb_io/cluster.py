"""Synchronous client for the public cluster HTTP API."""

from __future__ import annotations

import json
import re
import time
import uuid
from urllib.error import HTTPError
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, Request, build_opener


class ClusterError(Exception):
    def __init__(self, status, envelope):
        self.status = status
        self.envelope = envelope
        super().__init__(
            f"HTTP {status}: {envelope.get('message', 'invalid response')}"
        )


class _NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


class ClusterClient:
    """Returns the complete response, including read and commit tokens."""

    def __init__(self, origin: str, token: str, timeout: float = 45):
        parsed = urlsplit(origin)
        if (
            parsed.scheme not in ("http", "https")
            or not parsed.hostname
            or parsed.username is not None
            or parsed.password is not None
            or parsed.path not in ("", "/")
            or parsed.query
            or parsed.fragment
        ):
            raise ValueError("expected an HTTP(S) origin")
        self.origin = origin.rstrip("/")
        self._token = token
        self.timeout = timeout
        self._opener = build_opener(_NoRedirect())

    @staticmethod
    def new_key() -> str:
        return f"v2-{time.time_ns() // 1_000_000}-{uuid.uuid4().hex}"

    def execute(self, operation: str, body: dict, key: str | None = None) -> dict:
        if not re.fullmatch(r"[a-z_]+/[a-z_]+", operation):
            raise ValueError("expected category/action")
        headers = {
            "Authorization": f"Bearer {self._token}",
            "Content-Type": "application/json",
        }
        if key is not None:
            headers["Idempotency-Key"] = key
        req = Request(
            f"{self.origin}/v2/vectordb/{operation}",
            json.dumps(body, allow_nan=False).encode(),
            headers,
        )
        try:
            with self._opener.open(req, timeout=self.timeout) as resp:
                envelope = json.load(resp)
                if envelope.get("code") != 0:
                    raise ClusterError(resp.status, envelope)
                return envelope
        except HTTPError as error:
            with error:
                try:
                    body = json.load(error)
                except ValueError:
                    body = {"message": error.reason}
            raise ClusterError(error.code, body) from error
