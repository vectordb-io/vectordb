#!/usr/bin/env python3
"""Repeat bounded-key updates/deletes, Strong queries, compaction and verified backups."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import time
import uuid

import cluster_m5_smoke as local
from cluster_smoke import ROOT, ENV

sys.path.insert(0, str(ROOT / "python/vectordb_io"))
from cluster import ClusterClient, ClusterError


def percentile(values, quantile):
    return sorted(values)[int((len(values) - 1) * quantile)]


def soak(args, origin, cluster, env, folder):
    client = ClusterClient(origin, env["VECTORDB_CLUSTER_TOKEN"])
    coll = "soak_" + uuid.uuid4().hex[:12]
    base = {"collectionName": coll}
    rows = 64

    def call(op, body, write=False):
        key = client.new_key() if write else None
        until = time.monotonic() + 90
        while True:
            try:
                return client.execute(op, {**base, **body}, key)["data"]
            except ClusterError as exc:
                if exc.status != 503 or time.monotonic() >= until:
                    raise
                time.sleep(0.25)

    call("collections/create", {"shardCount": 4, "dimension": 2}, True)
    began = time.monotonic()
    cycle, backups = 0, 0
    timings, states = [], []
    while True:
        started = time.monotonic()
        batch = [
            {"id": i, "vector": [cycle % 16, i], "revision": cycle} for i in range(rows)
        ]
        call("entities/upsert", {"data": batch}, True)
        removed = cycle % rows
        call("entities/delete", {"ids": [removed]}, True)
        start_read = time.monotonic()
        result = call(
            "entities/query",
            {
                "filter": "id >= 0",
                "outputFields": ["id", "vector", "revision"],
                "limit": rows,
            },
        )
        timings.append((time.monotonic() - start_read) * 1000)
        assert sorted(result, key=lambda x: x["id"]) == [
            r for r in batch if r["id"] != removed
        ]
        if cycle % args.maintenance_every == 0:
            call("collections/compact", {})
            sample = call("collections/storage_stats", {})
            assert sample["walLow"] > 0 or sample["checkpointOffset"] < 65
            assert sample["walHigh"] - sample["walLow"] <= 256, sample
            # 63 live 2D rows; dedup receipts are allowed to grow to their 16 MiB cap.
            assert (
                sample["objectBytes"] < 20 * 1024 * 1024 and sample["objectCount"] < 128
            ), sample
            states.append(sample)
        if cycle % args.backup_every == 0:
            # Retain a single independent archive per observation; snapshot space belongs
            # to backup retention. TemporaryDirectory removes only this run's own copies.
            with tempfile.TemporaryDirectory(dir=folder) as stage:
                archive = Path(stage) / "archive"
                subprocess.run(
                    [
                        str(args.binary),
                        "--role",
                        "backup",
                        "--archive",
                        str(archive),
                        "--gateway",
                        origin,
                    ],
                    env={**env, "VECTORDB_CLUSTER": cluster},
                    check=True,
                    capture_output=True,
                    timeout=300,
                )
                catalog = (archive / "catalog.json").read_bytes()
                assert (
                    hashlib.sha256(catalog).hexdigest()
                    == (archive / "COMPLETE").read_text()
                )
                manifest = json.loads(catalog)
                cid = next(
                    k
                    for k, c in manifest["directory"]["collections"].items()
                    if c["name"] == coll
                )
                content = (archive / (cid + ".json")).read_bytes()
                assert (
                    hashlib.sha256(content).hexdigest() == manifest["collections"][cid]
                )
                assert sorted(
                    [r["body"] for r in json.loads(content)["rows"]],
                    key=lambda x: x["id"],
                ) == sorted(result, key=lambda x: x["id"])
                backups += 1
        cycle += 1
        elapsed = time.monotonic() - began
        if elapsed >= args.seconds:
            break
        time.sleep(
            max(
                0,
                min(
                    args.interval - (time.monotonic() - started), args.seconds - elapsed
                ),
            )
        )
    report = dict(
        topology="single-host coordinated"
        if args.local
        else "operator-supplied cluster",
        durationSeconds=round(elapsed, 3),
        cycles=cycle,
        verifiedBackups=backups,
        rowsPerUpsert=rows,
        verifiedRowsPerRead=rows - 1,
        queryP50Ms=round(statistics.median(timings), 3),
        queryP95Ms=round(percentile(timings, 0.95), 3),
        queryP99Ms=round(percentile(timings, 0.99), 3),
        storageSamples=len(states),
        maxCheckpointObjects=max(s["objectCount"] for s in states),
        minObjectBytes=min(s["objectBytes"] for s in states),
        maxObjectBytes=max(s["objectBytes"] for s in states),
        maxRetainedWalOffsets=max(s["walHigh"] - s["walLow"] for s in states),
        firstStorage=states[0],
        lastStorage=states[-1],
    )
    output = json.dumps(report, indent=2)
    print(output, flush=True)
    if args.report:
        with args.report.open("x") as stream:
            stream.write(output + "\n")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--local",
        action="store_true",
        help="start disposable TLS compute processes on the dedicated Compose backends",
    )
    mode.add_argument("--gateway", help="gateway of a dedicated release-test cluster")
    p.add_argument("--seconds", type=int, default=72 * 3600)
    p.add_argument("--interval", type=float, default=30)
    p.add_argument("--maintenance-every", type=int, default=10)
    p.add_argument("--backup-every", type=int, default=120)
    p.add_argument("--report", type=Path)
    p.add_argument(
        "--binary", type=Path, default=ROOT / "target/release/vectordb-cluster"
    )
    args = p.parse_args()
    if (
        min(args.seconds, args.maintenance_every, args.backup_every) <= 0
        or args.interval < 0
    ):
        p.error("durations and sample intervals must be positive")
    with tempfile.TemporaryDirectory(prefix="vectordb-m6-soak-") as folder:
        if args.local:
            local.TMP = Path(folder)
            local.BINARY = args.binary
            try:
                local.certificates()
                local.start("c1", "coordinator", "a")
                local.start("q1", "query-node", "a")
                local.start("q2", "query-node", "b")
                addr = local.start("w1", "node", "b")
                soak(args, f"http://127.0.0.1:{addr}", local.CLUSTER, ENV, folder)
            finally:
                for node in local.PROCESSES:
                    local.stop(node)
        else:
            soak(args, args.gateway, os.environ["VECTORDB_CLUSTER"], os.environ, folder)


if __name__ == "__main__":
    main()
