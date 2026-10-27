# Embedded Python package (Alpha)

VectorDB runs inside your Python process using the existing Rust engine and
bundled SQLite. No HTTP server or external database is needed. The local
distribution name is `vectordb_io`; the Python import is `vectordb_io`.
This package has not been published to PyPI.

Once published, install and import it with the same name:

```sh
pip install vectordb_io
```

```python
from vectordb_io import VectorDB
```

For now, use the local source or wheel installation below. The package owns
only the `vectordb_io` import namespace, so it does not overwrite another
project's `vectordb` module. If you installed the earlier local prototype,
uninstall `vectordb-embedded` before installing this renamed package and update
your imports to `vectordb_io`.

## Local installation

From the repository root, with CPython 3.9+, Rust 1.89+, Cargo, and a C compiler
(Xcode Command Line Tools on macOS):

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install .
python examples/embedded.py
python -m unittest discover -s tests/python -v
```

On Windows, activate with `.venv\Scripts\activate` instead. Windows is not yet
part of the tested release matrix. First-time source builds download maturin
and Rust dependencies. A source installation compiles the native extension.

To build a reusable wheel and install it into another environment:

```sh
python -m pip wheel . --no-deps --wheel-dir dist
python3 -m venv .venv-wheel
.venv-wheel/bin/python -m pip install --no-index --find-links dist vectordb_io
.venv-wheel/bin/python -m unittest discover -s tests/python -v
```

A matching precompiled wheel needs no Rust/C toolchain at installation or
runtime. Wheels are specific to the OS and CPU architecture; the `abi3-py39`
build targets standard CPython 3.9+ (not free-threaded Python or PyPy).
Do not assume a locally built wheel works on other operating systems.

## Quick start

```python
from vectordb_io import VectorDB

with VectorDB("./data/demo.sqlite") as db:
    db.create_collection("demo", dimension=3)
    db.insert("demo", [
        {"id": 1, "vector": [1.0, 0.0, 0.0], "title": "Rust"},
        {"id": 2, "vector": [0.0, 1.0, 0.0], "title": "Python"},
    ])
    print(db.search("demo", [[1.0, 0.0, 0.0]], limit=1, output_fields=["title"]))
    print(db.get("demo", [1, 2]))
    print(db.query("demo", "id >= 2", output_fields=["title"]))
    db.upsert("demo", [{"id": 2, "vector": [0, 1, 0], "title": "Updated"}])
    db.delete("demo", "id == 1")

with VectorDB("./data/demo.sqlite") as db:
    assert db.get("demo", 1) == []
    assert db.get("demo", 2, output_fields=["title"])[0]["title"] == "Updated"
```

Use a new path or drop the existing collection before rerunning collection
creation. `VectorDB()` and `VectorDB(":memory:")` create isolated, ephemeral
databases. Parent directories for a file database are created automatically.

## API and behavior

| Method | Behavior |
| --- | --- |
| `create_collection(name, dimension, ...)` | Dense FloatVector, FLAT index, COSINE by default; accepts `metric_type`, `id_type`, `auto_id`, `primary_field_name`, `vector_field_name` |
| `list_collections()` / `drop_collection(name)` | List or remove collections in the default database |
| `insert(collection, data)` | Atomic batch, 1–10,000 entities; returns `insertCount` and `insertIds` |
| `upsert(collection, data)` | Full-entity replacement, not a patch; returns `upsertCount` and `upsertIds` |
| `get(collection, ids, ...)` | Scalar or list of IDs; missing IDs omitted |
| `query(collection, filter="", ...)` | Scalar filtering; supports `output_fields`, `limit` and `offset` |
| `search(collection, vectors, ...)` | 1–128 query vectors; supports `limit`, `filter`, `output_fields` |
| `delete(collection, filter)` | Requires a nonempty filter; returns `deleteCount` |
| `execute(operation, request)` | Raw engine API with camelCase request keys; returns data without the HTTP `{code,data}` envelope |
| `flush()` / `backup(path)` | WAL checkpoint / consistent snapshot; backup cannot overwrite a file |
| `close()` / `closed` | Deterministic resource release / closed state |

`get` and `query` default to at most 100 records; use `limit` and `offset` for
pagination. `search` defaults to 10 hits per vector. Results use the engine's
field names: `id`, `distance`, and projected fields. Multiple query vectors
produce a flat list with `queryIndex` identifying the input vector. COSINE and
IP scores are larger for better matches; L2 is squared distance, smaller is
better. Output fields default to the engine's projection (see `docs/api.md`).

All data crosses a JSON boundary. Use ordinary Python lists/dicts/scalars;
NumPy arrays are not accepted directly. Signed Int64 IDs retain integer
precision. NaN and infinity are rejected. Applications generate embeddings.
Advanced schemas, indexes, databases and partitions remain accessible through
`execute`; the convenience API operates on the default database.

## Lifetime, concurrency, and durability

- Use a `with` block or call `close()`. Close is idempotent and releases the
  SQLite connection and file lock. Operations after close raise `ClosedError`.
- Successful writes commit before returning; `flush()` is not required to save
  data. SQLite WAL/FULL durability still depends on the filesystem/hardware.
- One engine instance owns a file. Opening the same file twice, including from
  another process or the HTTP server, raises `ConflictError`. Do not bypass the
  lock using path aliases or modify the database with external tools.
- Threads can share an instance; calls serialize behind a mutex. Rust database
  work and mutex waits release the GIL. Close waits for active operations; a
  racing call may either finish before close or raise `ClosedError`.
- Do not use handles inherited through `fork()`. Such calls raise
  `ConflictError`. Prefer spawning workers and opening independent database
  files in each process. There is no async API in this version.
- Existing engine capacity limits still apply: this package does not change
  index rebuilding, JSON storage overhead, or single-instance concurrency.

## Errors and validation

All engine errors derive from `VectorDBError`: `InvalidArgumentError`,
`NotFoundError`, `ConflictError`, `UnsupportedError`, `StorageError`, and
`ClosedError`. Python signature/type errors may also raise normal `TypeError`.

The integration suite runs against the installed package and covers CRUD,
search, Int64/Unicode round trips, rollback, reopening, abrupt process exit,
exclusive locks, context cleanup, snapshots, threaded calls, and fork guards.
It uses temporary databases and does not touch `data/`.

Existing server builds remain Python-independent via the workspace's default
members or `cargo build -p vectordb-server`. Workspace-wide Rust checks also
include the Python crate and need a discoverable Python interpreter; set
`PYO3_PYTHON` to its absolute path if needed. The extension-module linking
feature is enabled by maturin only. The binding crate is tested through the
installed-wheel suite rather than a standalone Rust test executable.
