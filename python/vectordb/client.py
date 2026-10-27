"""Synchronous convenience API over the embedded Rust engine."""

from __future__ import annotations

import json
import os
from typing import Any

from ._native import ClosedError, InvalidArgumentError, NativeEngine


def _path(value: str | os.PathLike[str]) -> str:
    path = os.fspath(value)
    if not isinstance(path, str) or not path or "\0" in path:
        raise InvalidArgumentError("path must be a nonempty string without NUL characters")
    return path


class VectorDB:
    """Open a file database, or an isolated memory database with None/':memory:'.

    Calls on one instance are serialized. Use close() or a with block to release
    its file lock. Never share a handle across processes or fork().
    """

    def __init__(self, path: str | os.PathLike[str] | None = None) -> None:
        filename = None if path is None else _path(path)
        self._engine = NativeEngine(None if filename == ":memory:" else filename)

    @property
    def closed(self) -> bool:
        return self._engine.closed

    def close(self) -> None:
        """Wait for in-flight work, release resources; repeated calls are harmless."""
        self._engine.close()

    def __enter__(self) -> VectorDB:
        if self.closed:
            raise ClosedError("database is closed")
        return self

    def __exit__(self, exc_type: Any, exc_value: Any, traceback: Any) -> None:
        self.close()

    def execute(self, operation: str, request: dict[str, Any]) -> Any:
        """Execute an engine operation; returns data directly, without an HTTP envelope.

        Request keys use the engine's camelCase contract (see docs/api.md).
        Values must be JSON-compatible; NaN and infinity are rejected.
        """
        if not isinstance(operation, str) or not isinstance(request, dict):
            raise InvalidArgumentError("operation must be a string and request must be a dict")
        try:
            payload = json.dumps(request, allow_nan=False, ensure_ascii=True)
        except (TypeError, ValueError, OverflowError) as error:
            raise InvalidArgumentError(str(error)) from error
        return json.loads(self._engine.execute_json(operation, payload))

    def create_collection(
        self, name: str, dimension: int, *, metric_type: str = "COSINE",
        id_type: str = "Int64", auto_id: bool = False,
        primary_field_name: str = "id", vector_field_name: str = "vector",
    ) -> dict[str, Any]:
        """Create a dense-vector collection with dynamic fields and a FLAT index."""
        return self.execute("collections/create", {
            "collectionName": name, "dimension": dimension, "metricType": metric_type,
            "idType": id_type, "autoID": auto_id,
            "primaryFieldName": primary_field_name, "vectorFieldName": vector_field_name,
        })

    def list_collections(self) -> list[str]:
        return self.execute("collections/list", {})

    def drop_collection(self, name: str) -> dict[str, Any]:
        return self.execute("collections/drop", {"collectionName": name})

    def insert(self, collection: str, data: list[dict[str, Any]]) -> dict[str, Any]:
        """Insert 1–10,000 entities atomically; duplicate primary keys fail."""
        return self.execute("entities/insert", {"collectionName": collection, "data": data})

    def upsert(self, collection: str, data: list[dict[str, Any]]) -> dict[str, Any]:
        """Insert or replace complete entities (not a partial field update)."""
        return self.execute("entities/upsert", {"collectionName": collection, "data": data})

    def get(
        self, collection: str, ids: int | str | list[int | str], *,
        output_fields: list[str] | None = None, limit: int = 100, offset: int = 0,
    ) -> list[dict[str, Any]]:
        """Fetch matching IDs, with pagination; missing IDs are omitted."""
        request: dict[str, Any] = {
            "collectionName": collection, "id": ids, "limit": limit, "offset": offset,
        }
        if output_fields is not None:
            request["outputFields"] = output_fields
        return self.execute("entities/get", request)

    def query(
        self, collection: str, filter: str = "", *, output_fields: list[str] | None = None,
        limit: int = 100, offset: int = 0,
    ) -> list[dict[str, Any]]:
        """Query using the engine's scalar filter syntax, e.g. 'id >= 2'."""
        request: dict[str, Any] = {
            "collectionName": collection, "filter": filter, "limit": limit, "offset": offset,
        }
        if output_fields is not None:
            request["outputFields"] = output_fields
        return self.execute("entities/query", request)

    def delete(self, collection: str, filter: str) -> dict[str, Any]:
        """Delete matching entities; an empty filter is rejected."""
        return self.execute("entities/delete", {"collectionName": collection, "filter": filter})

    def search(
        self, collection: str, vectors: list[list[float]], *, limit: int = 10,
        filter: str = "", output_fields: list[str] | None = None,
    ) -> list[dict[str, Any]]:
        """Search 1–128 vectors; results are flat, with queryIndex for multiple queries.

        The distance field follows the collection metric: COSINE/IP are larger
        for better matches, L2 is squared distance and smaller is better.
        """
        request: dict[str, Any] = {
            "collectionName": collection, "data": vectors, "limit": limit, "filter": filter,
        }
        if output_fields is not None:
            request["outputFields"] = output_fields
        return self.execute("entities/search", request)

    def flush(self) -> None:
        """Checkpoint the WAL. Successful writes are already committed without this."""
        self._engine.flush()

    def backup(self, path: str | os.PathLike[str]) -> None:
        """Create a consistent SQLite snapshot; the destination must not exist."""
        self._engine.backup(_path(path))
