# VectorDB · Rust

**English | [简体中文](README.zh-CN.md) | [日本語](README.ja.md) | [Español](README.es.md) | [Português do Brasil](README.pt-BR.md)**

A standalone vector database reimplemented in Rust, with an embeddable engine, an HTTP service, transactional persistence, and a separate static website.

**Status: 0.1.0 Alpha.** Inspired by the Milvus data model and REST v2 request style, VectorDB implements a usable set of standalone features. Most of the distributed infrastructure in Milvus is not implemented, and VectorDB is not a drop-in replacement for Milvus / PyMilvus. See the [feature comparison](docs/compatibility.md) for the exact scope. The previous C++ implementation, documentation, and website have been removed.

## Quick start

Requires Rust 1.89+, Cargo, and a C compiler toolchain (to compile the bundled SQLite). Use Xcode Command Line Tools on macOS or the system C compiler on Linux. No external database service is required.

```sh
cargo build --release --locked
./target/release/vectordb serve
```

The server listens on `127.0.0.1:19530` by default and stores data in `data/vectordb.sqlite`. In another terminal:

```sh
curl http://127.0.0.1:19530/healthz
bash examples/quickstart.sh
```

The example creates a `demo` collection, inserts two entities, searches for similar vectors, and applies scalar filters. Before running it again, delete `demo` or use a different collection name.

## Current capabilities

- Multiple databases, collections, explicit schemas / quick collection creation, partitions, collection aliases, renaming, loading, and releasing.
- Int64 / VarChar primary keys and automatic IDs; scalar fields, JSON, Array, nullable fields, default values, and dynamic fields.
- Batch insert / full-entity upsert, query / get / delete, counting, sorting, and pagination.
- FloatVector, SparseFloatVector, BinaryVector; COSINE, IP, L2 (squared distance), HAMMING, and JACCARD.
- FLAT exact search, IVF_FLAT, HNSW; scalar pre-filtering, range search, grouping search, and multiple query vectors.
- Multi-vector search fusion: RRF and normalized weighted ranking.
- SQLite WAL with FULL synchronization; batch transactions, crash recovery, an exclusive process lock, snapshot backups, and streaming JSONL import / export.
- Bearer tokens, request size and concurrency queue limits, health checks, Prometheus request / error counters, and graceful shutdown.

Embeddings are not generated automatically; applications must generate and submit vectors. The database logic, filters, index algorithms, and HTTP service are written in Rust. SQLite, implemented in C, is the embedded persistence dependency.

## Request examples

```sh
curl -sS localhost:19530/v2/vectordb/collections/create \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","dimension":3}'

curl -sS localhost:19530/v2/vectordb/entities/insert \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","data":[
    {"id":1,"vector":[1,0,0],"title":"Rust"},
    {"id":2,"vector":[0,1,0],"title":"Storage"}
  ]}'

curl -sS localhost:19530/v2/vectordb/entities/search \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","data":[[1,0,0]],"limit":1,"outputFields":["title"]}'
```

Response: `{"code":0,"data":[{"id":1,"title":"Rust","distance":1.0}]}`.

## Embedded Python (Alpha)

From the repository root, install with CPython 3.9+ and the Rust/C toolchain
described above. A matching precompiled wheel needs no Rust/C compiler. This
package is currently built locally and has not been published to PyPI.
The distribution and import name are both `vectordb_io`; after publication,
install with `pip install vectordb_io`.
Release wheels target Apple Silicon Macs (macOS 11+) and Linux x86_64
(glibc 2.17+). See the [platform validation record](docs/python-validation.md).

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install .
python examples/embedded.py
python -m unittest discover -s tests/python -v
```

```python
from vectordb_io import VectorDB

with VectorDB("./data/python-demo.sqlite") as db:
    db.create_collection("demo", dimension=3)
    db.insert("demo", [{"id": 1, "vector": [1, 0, 0]}])
    print(db.search("demo", [[1, 0, 0]], limit=1))
```

Supports file and memory databases, CRUD, search, full-entity upsert, snapshots,
and raw `execute()` operations. Writes commit before returning; the context
manager closes the database and releases its file lock. Use a new collection
name or drop the existing collection before rerunning. See the
[Python guide](docs/python.md) for installation, API and concurrency details.

## Embedded Rust

```rust
use serde_json::json;
use vectordb_engine::Engine;

fn main() -> vectordb_engine::Result<()> {
    let mut db = Engine::memory()?;
    db.execute("collections/create", json!({"collectionName":"demo","dimension":3}))?;
    db.execute("entities/insert", json!({"collectionName":"demo","data":[
        {"id":1,"vector":[1,0,0]}
    ]}))?;
    let hits = db.execute("entities/search", json!({
        "collectionName":"demo","data":[[1,0,0]],"limit":1
    }))?;
    println!("{hits}");
    Ok(())
}
```

Run the complete example included in the repository: `cargo run -p vectordb-engine --example embedded`.

## Validation

```sh
cargo fmt --all --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo build --workspace --locked
python3 scripts/smoke.py
python3 scripts/check_website.py
```

Tests cover transaction rollback, schema validation, filtering, primary key precision, load state, database / partition isolation, index refresh, ANN comparisons, hybrid search, snapshots, exclusive locking, database corruption, HTTP authentication, and concurrent writes. `smoke.py` starts a real process with a temporary database to verify that acknowledged writes can be recovered after SIGKILL. It does not use actual data in `data/`.

## Documentation

- [API and request contracts](docs/api.md)
- [Architecture, data reliability, and performance limits](docs/architecture.md)
- [Milvus feature comparison and roadmap](docs/compatibility.md)
- [Deployment, authentication, backup, and recovery](docs/operations.md)
- [Validation records](docs/validation.md)
- [Website and domain publishing](website/README.md)

Directories: `crates/engine` — database engine; `crates/server` — HTTP / CLI; `docs` — documentation; `examples` — examples; `website` — website. The website is deployed separately to GitHub Pages at [vectordb.io](https://vectordb.io/).
