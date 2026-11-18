"""A local, embedded vector database. No server is required."""

from ._native import (
    ClosedError,
    ConflictError,
    InvalidArgumentError,
    NotFoundError,
    StorageError,
    UnsupportedError,
    VectorDBError,
    __version__,
)
from .client import VectorDB
from .cluster import ClusterClient, ClusterError

__all__ = [
    "ClusterClient", "ClusterError", "VectorDB", "VectorDBError", "InvalidArgumentError", "NotFoundError",
    "ConflictError", "UnsupportedError", "StorageError", "ClosedError", "__version__",
]
