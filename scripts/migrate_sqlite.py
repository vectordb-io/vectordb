#!/usr/bin/env python3
"""Convert a stopped SQLite v1 snapshot into a checksummed cluster restore archive."""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sqlite3
import struct
import uuid
import time


def write_json(path, obj):
    data = json.dumps(
        obj, ensure_ascii=False, allow_nan=False, separators=(",", ":")
    ).encode()
    with path.open("xb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    return hashlib.sha256(data).hexdigest()


def migrate(source, output, dtype, shards):
    cluster = "sqlite-" + uuid.uuid4().hex
    directory = dict(format=1, clock_ms=0, databases=[], collections={}, aliases={})
    archive = dict(
        format=1, cluster=cluster, directory=directory, collections={}, receipts={}
    )
    if any(Path(str(source) + suffix).exists() for suffix in ("-wal", "-journal")):
        raise ValueError("use a self-contained snapshot from vectordb backup")
    con = sqlite3.connect(
        source.resolve(strict=True).as_uri() + "?mode=ro&immutable=1", uri=True
    )
    con.execute("BEGIN")
    if con.execute("PRAGMA user_version").fetchone() != (1,):
        raise ValueError("expected VectorDB SQLite format 1")
    if (
        con.execute("PRAGMA integrity_check").fetchall() != [("ok",)]
        or con.execute("PRAGMA foreign_key_check").fetchone()
    ):
        raise ValueError("SQLite integrity check failed")
    output.mkdir(mode=0o700)
    directory["databases"] = [
        db for (db,) in con.execute("SELECT name FROM databases ORDER BY name")
    ]
    now_ms = time.time_ns() // 1_000_000
    has_p1 = con.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='vectordb_p1_keys'"
    ).fetchone()
    total = 0
    max_error = 0.0
    for cid, db, name, schema, indexes, next_id, loaded in con.execute(
        "SELECT id, db, name, schema_json, indexes_json, next_id, loaded FROM collections ORDER BY id"
    ):
        if (
            not loaded
            or con.execute(
                "SELECT 1 FROM partitions WHERE cid=? AND loaded=0", (cid,)
            ).fetchone()
        ):
            raise ValueError(
                "unloaded collection or partition requires an explicit load before migration"
            )
        physical = "c" + uuid.uuid4().hex
        scope = dict(cluster=cluster, collection=physical, generation=uuid.uuid4().hex)
        schema = json.loads(schema)
        partitions = [
            p
            for (p,) in con.execute(
                "SELECT name FROM partitions WHERE cid=? ORDER BY name", (cid,)
            )
        ]
        spec = dict(
            format=2,
            name=physical,
            schema=schema,
            dtype=dtype,
            indexes=json.loads(indexes),
            next_id=next_id,
            partitions=partitions,
            shard_count=shards,
        )
        create = dict(
            collectionName=physical,
            dbName="default",
            schema=schema,
            vectorDtype=dtype,
            indexParams=spec["indexes"],
            shardCount=shards,
        )
        directory["collections"][physical] = dict(
            database=db,
            name=name,
            scope=scope,
            state="Active",
            create_request=create,
            create_id="migration",
            drop_id=None,
            spec=spec,
            spec_offset=1,
            shard_count=shards,
            query_routes=None,
        )
        if has_p1:
            for (key,) in con.execute(
                "SELECT request_id FROM vectordb_p1_keys WHERE collection=?", (name,)
            ):
                match = re.fullmatch(r"v2-([0-9]+)-[A-Za-z0-9_-]+", key)
                if not match:
                    continue
                issued = int(match.group(1))
                if issued + 86_400_000 <= now_ms or issued > 2**63 - 1 - 86_460_000:
                    continue
                bucket = (issued + 86_460_000) // 60_000 * 60_000
                identity = json.dumps(
                    [db, name, key], separators=(",", ":"), ensure_ascii=False
                ).encode()
                route = f"requests/{bucket:020d}/{hashlib.sha256(identity).hexdigest()}"
                archive["receipts"][route] = dict(
                    digest="retired-p1-key", target=scope, result=None
                )
                expirations = directory.setdefault("receipt_expirations", {})
                expirations[bucket] = expirations.get(bucket, 0) + 1
        rows = []
        for key, partition, body in con.execute(
            "SELECT pk,partition,body FROM entities WHERE cid=? ORDER BY pk", (cid,)
        ):
            body = json.loads(body)
            if dtype == "Float32":
                for f in schema["fields"]:
                    if f["dataType"] == "FloatVector":
                        vec = body[f["fieldName"]]
                        quantized = [
                            struct.unpack("<f", struct.pack("<f", n))[0] for n in vec
                        ]
                        if not all(map(math.isfinite, quantized)):
                            raise ValueError("Float32 overflow")
                        max_error = max(
                            max_error, max(abs(n - q) for n, q in zip(vec, quantized))
                        )
                        body[f["fieldName"]] = quantized
            rows.append(dict(key=key, partition=partition, version=1, body=body))
        total += len(rows)
        archive["collections"][physical] = write_json(
            output / (physical + ".json"),
            dict(scope=scope, spec=spec, rows=rows, results={}, clock_ms=0),
        )
        for (alias,) in con.execute("SELECT name FROM aliases WHERE cid=?", (cid,)):
            directory["aliases"][db + "/" + alias] = physical
    con.close()
    names = directory["databases"] + [
        c["name"] for c in directory["collections"].values()
    ]
    names.extend(k.split("/", 1)[1] for k in directory["aliases"])
    if (
        len(directory["collections"]) > 16
        or len(directory["databases"]) > 32
        or any(not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]{0,63}", n) for n in names)
    ):
        raise ValueError("legacy names or collection counts exceed cluster limits")
    if len(archive["receipts"]) > 32768:
        raise ValueError("retained P1 request keys exceed target receipt quota")
    digest = write_json(output / "catalog.json", archive)
    with (output / "COMPLETE").open("x") as stream:
        stream.write(digest)
        stream.flush()
        os.fsync(stream.fileno())
    print(
        json.dumps(
            dict(
                rows=total,
                collections=len(archive["collections"]),
                retiredP1Keys=len(archive["receipts"]),
                vectorDtype=dtype,
                maxAbsoluteVectorError=max_error,
                catalogSha256=digest,
            )
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--dtype", choices=["Float32", "Float64"], default="Float64")
    parser.add_argument("--shards", type=int, choices=[1, 2, 4, 8, 16, 32], default=16)
    args = parser.parse_args()
    migrate(args.source, args.output, args.dtype, args.shards)
