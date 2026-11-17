#!/usr/bin/env python3
"""M4 real-backend sharding acceptance and optional fixed-workload Query scaling."""
import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import signal
import subprocess
import tempfile
import time
import uuid
from datetime import datetime, timezone
from cluster_smoke import ROOT, ENV, api, command, port, wait_ready

PROCESSES = []
LOGS = []
TMP = None
BINARY = Path(os.getenv("VECTORDB_M4_BINARY", str(ROOT / "target/debug/vectordb-cluster")))


def start(cluster, role="query-node", delay=0, address=None):
    address = address or port()
    env = {**ENV, "VECTORDB_CLUSTER": cluster}
    if delay:
        env["VECTORDB_TEST_SHARD_DELAY_MS"] = str(delay)
    path = Path(TMP) / f"{role}-{address}-{len(LOGS)}.log"
    with path.open("w") as stream:
        process = subprocess.Popen([str(BINARY), "--role", role, "--bind", f"127.0.0.1:{address}", "--advertise", f"http://127.0.0.1:{address}"], env=env, stdout=stream, stderr=stream)
    PROCESSES.append(process)
    LOGS.append(path)
    wait_ready(address, process)
    return process, address


def stop(process):
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)


def call(address, op, body, key=None, expected=200):
    status, result = api(address, "/v2/vectordb/" + op, body, key)
    assert status == expected, (op, status, result)
    return result


def route(address, name, replicas, epoch):
    return call(address, "collections/set_query_routes", {"collectionName": name, "expectedEpoch": epoch, "replicas": [[f"http://127.0.0.1:{p}" for p in group] for group in replicas]}, uuid.uuid4().hex)["data"]["epoch"]


def acceptance():
    cluster = "m4-" + uuid.uuid4().hex
    writer, gateway = start(cluster, "node")
    qa, a = start(cluster)
    qb, b = start(cluster)
    slow, slow_port = start(cluster, delay=2000)
    request = {"collectionName": "items", "shardCount": 8, "vectorDtype": "Float64", "schema": {"fields": [{"fieldName": "id", "dataType": "Int64", "isPrimary": True}, {"fieldName": "rank", "dataType": "Int64"}, {"fieldName": "v", "dataType": "FloatVector", "elementTypeParams": {"dim": 2}}, {"fieldName": "w", "dataType": "FloatVector", "elementTypeParams": {"dim": 2}}]}, "indexParams": [{"fieldName": "v", "metricType": "L2"}, {"fieldName": "w", "metricType": "IP"}]}
    call(gateway, "collections/create", request, "create")
    rows = [{"id": n, "rank": n % 7, "v": [n / 100., 1.], "w": [n / 200., -1.]} for n in range(1000)]
    for offset in range(0, 1000, 200):
        call(gateway, "entities/insert", {"collectionName": "items", "data": rows[offset:offset + 200]}, f"batch{offset}")
    queries = [
        ("entities/count", {"filter": "rank >= 3"}),
        ("entities/query", {"outputFields": ["rank"], "orderByFields": ["rank:desc"], "offset": 13, "limit": 31}),
        ("entities/query", {"outputFields": ["count(*)"], "filter": "rank == 2"}),
        ("entities/get", {"id": [3, 41, 999, 1001], "outputFields": ["v"]}),
        ("entities/search", {"annsField": "v", "data": [[0., 0.], [2., 1.]], "limit": 10, "offset": 3, "filter": "rank >= 2", "outputFields": ["rank"]}),
        ("entities/hybrid_search", {"search": [{"annsField": "v", "data": [[0., 0.]], "limit": 30}, {"annsField": "w", "data": [[1., 0.]], "limit": 30}], "rerank": {"strategy": "rrf", "params": {"k": 10}}, "limit": 15}),
        ("entities/hybrid_search", {"search": [{"annsField": "v", "data": [[0., 0.]], "limit": 30}, {"annsField": "w", "data": [[1., 0.]], "limit": 30}], "rerank": {"strategy": "weighted", "params": {"weights": [0.7, 0.3]}}, "limit": 15}),
    ]
    expected = [call(gateway, op, {"collectionName": "items", **body})["data"] for op, body in queries]
    checkpoint = call(gateway, "collections/flush", {"collectionName": "items"})["data"]["manifest"]
    epoch = route(gateway, "items", [[a] if i < 4 else [b] for i in range(8)], 0)
    print("M4: assigned-only loading, exact global top-k/count/order/pagination/hybrid", flush=True)
    for (op, body), truth in zip(queries, expected):
        result = call(gateway, op, {"collectionName": "items", **body})
        assert result["data"] == truth, (op, result["data"], truth)
        assert result["routeEpoch"] == epoch
    stats = [call(p, "collections/shard_status", {"collectionName": "items"})["data"] for p in [a, b]]
    assert sum(s["rows"] for s in stats) == 1000
    assert all(0 < s["rows"] < 1000 and s["fullCollectionSessions"] == 0 for s in stats)
    assert [sorted(s["shards"]) for s in stats] == [list(range(4)), list(range(4, 8))]
    call(gateway, "collections/set_query_routes", {"collectionName": "items", "expectedEpoch": 0, "replicas": [[f"http://127.0.0.1:{a}"]] * 8}, "stale-routes", 409)
    print("M4: atomic batches across shards under concurrent writes and reads", flush=True)
    # FNV v1 maps these adjacent integer keys to different shards.
    def pair(version):
        return [{**rows[i], "rank": version} for i in [100, 101]]
    call(gateway, "entities/upsert", {"collectionName": "items", "data": pair(50)}, "pair-initial")
    def update_pairs():
        for version in range(51, 61):
            call(gateway, "entities/upsert", {"collectionName": "items", "data": pair(version)}, f"pair-{version}")
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        updates = pool.submit(update_pairs)
        for i in range(12):
            snapshot = call([gateway, a, b][i % 3], "entities/get", {"collectionName": "items", "id": [100, 101], "outputFields": ["rank"]})["data"]
            assert len(snapshot) == 2 and snapshot[0]["rank"] == snapshot[1]["rank"], snapshot
        updates.result()
    call(gateway, "entities/upsert", {"collectionName": "items", "data": [rows[100], rows[101]]}, "pair-restore")
    print("M4: shared WAL tail, delete visibility and concurrent snapshot requests", flush=True)
    call(gateway, "entities/upsert", {"collectionName": "items", "data": [{"id": 500, "rank": 4, "v": [0., 0.], "w": [100., 0.]}]}, "update")
    call(gateway, "entities/delete", {"collectionName": "items", "ids": [1, 2, 3]}, "delete")
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(call, gateway, "entities/count", {"collectionName": "items"}) for _ in range(8)]
        assert all(f.result()["data"]["count"] == 997 for f in futures)
    hits = call(gateway, "entities/search", {"collectionName": "items", "annsField": "v", "data": [[0., 0.]], "limit": 10})["data"]
    assert hits[0]["id"] == 500 and not {1, 2, 3}.intersection(h["id"] for h in hits)
    print("M4: pinned manifest, migration with in-flight views, complete-snapshot retry", flush=True)
    epoch = route(gateway, "items", [[slow_port] if i < 4 else [b] for i in range(8)], epoch)
    with concurrent.futures.ThreadPoolExecutor() as pool:
        future = pool.submit(call, gateway, "entities/count", {"collectionName": "items", "timeoutMs": 10000})
        wait_until = time.monotonic() + 5
        while call(slow_port, "collections/shard_status", {"collectionName": "items"})["data"]["rows"] == 0:
            assert time.monotonic() < wait_until, "slow replica did not capture its view"
            time.sleep(0.02)
        call(gateway, "collections/flush", {"collectionName": "items"})
        command("exec", "-T", "minio", "mcli", "stat", "test/vectordb/" + checkpoint["key"])
        epoch = route(gateway, "items", [[b] if i < 4 else [a] for i in range(8)], epoch)
        result = future.result(timeout=15)
        assert result["data"]["count"] == 997 and result["routeEpoch"] == epoch
    stats = [call(p, "collections/shard_status", {"collectionName": "items"})["data"] for p in [a, b]]
    assert [sorted(s["shards"]) for s in stats] == [list(range(4, 8)), list(range(4))]
    call(gateway, "collections/flush", {"collectionName": "items"})
    try:
        command("exec", "-T", "minio", "mcli", "stat", "test/vectordb/" + checkpoint["key"])
        raise AssertionError("retired checkpoint was not reclaimed after query pin release")
    except subprocess.CalledProcessError:
        pass
    print("M4: dead/slow replicas, bounded deadline, no partial success", flush=True)
    dead = port()
    epoch = route(gateway, "items", [[dead, a] if i < 4 else [dead, b] for i in range(8)], epoch)
    assert call(gateway, "entities/count", {"collectionName": "items", "timeoutMs": 10000})["data"]["count"] == 997
    epoch = route(gateway, "items", [[slow_port, a] if i < 4 else [slow_port, b] for i in range(8)], epoch)
    assert call(gateway, "entities/count", {"collectionName": "items", "timeoutMs": 2000})["data"]["count"] == 997
    epoch = route(gateway, "items", [[dead] if i == 0 else [a] for i in range(8)], epoch)
    started = time.monotonic()
    call(gateway, "entities/count", {"collectionName": "items", "timeoutMs": 250}, expected=503)
    assert time.monotonic() - started < 1.5
    epoch = route(gateway, "items", [[a] if i < 4 else [b] for i in range(8)], epoch)
    stop(qa)
    qa, a = start(cluster, address=a)
    assert call(gateway, "entities/count", {"collectionName": "items"})["data"]["count"] == 997
    assert call(a, "collections/shard_status", {"collectionName": "items"})["data"]["rows"] < 997
    call(gateway, "entities/query", {"collectionName": "items", "limit": 16000, "offset": 1000}, expected=400)
    print("PASS: M4 selective snapshots, shared WAL, global merge, replicas, migration, pins and deadlines", flush=True)
    for p in [writer, qa, qb, slow]:
        stop(p)


def benchmark():
    cluster = "m4-bench-" + uuid.uuid4().hex
    writer, gateway = start(cluster, "node")
    qa, a = start(cluster)
    qb, b = start(cluster)
    rng = random.Random(24301)
    count, dimension, nq, samples = 10000, 32, 16, 20
    call(gateway, "collections/create", {"collectionName": "bench", "dimension": dimension, "metricType": "L2", "shardCount": 16}, "create")
    rows = [{"id": n, "vector": [rng.random() for _ in range(dimension)]} for n in range(count)]
    for offset in range(0, count, 200):
        call(gateway, "entities/insert", {"collectionName": "bench", "data": rows[offset:offset+200]}, f"batch{offset}")
    query = {"collectionName": "bench", "data": [[rng.random() for _ in range(dimension)] for _ in range(nq)], "limit": 10}
    truth = call(gateway, "entities/search", query)["data"]
    call(gateway, "collections/flush", {"collectionName": "bench"})
    epoch = 0
    results = []
    for nodes in [1, 2]:
        if nodes == 2:
            stop(qa)
            qa, a = start(cluster, address=a)
        epoch = route(gateway, "bench", [[a] if nodes == 1 or i < 8 else [b] for i in range(16)], epoch)
        warm = time.monotonic()
        assert call(gateway, "entities/search", query)["data"] == truth
        warm_ms = (time.monotonic()-warm)*1000
        latencies = []
        started = time.monotonic()
        for _ in range(samples):
            t = time.monotonic()
            assert call(gateway, "entities/search", query)["data"] == truth
            latencies.append((time.monotonic()-t)*1000)
        elapsed = time.monotonic()-started
        stats = [call(p, "collections/shard_status", {"collectionName": "bench"})["data"] for p in ([a] if nodes == 1 else [a, b])]
        rss = [int(subprocess.check_output(["ps", "-o", "rss=", "-p", str(p.pid)], text=True).strip())*1024 for p in ([qa] if nodes == 1 else [qa, qb])]
        assert sum(s["rows"] for s in stats) == count
        assert all(s["fullCollectionSessions"] == 0 for s in stats)
        results.append({"query_nodes": nodes, "requests_per_second": samples/elapsed, "vectors_per_second": samples*nq/elapsed, "p50_ms": sorted(latencies)[samples//2], "p99_ms": max(latencies), "cold_query_ms": warm_ms, "query_process_rss_bytes": rss, "assigned_state": stats, "exact_match": True})
    report = {"format": 1, "created_at": datetime.now(timezone.utc).isoformat(), "seed": 24301, "rows": count, "dimension": dimension, "dtype": "Float32", "index": "FLAT", "shards": 16, "vectors_per_request": nq, "samples": samples, "concurrency": 1, "top_k": 10, "profile": "release" if "release" in str(BINARY) else "debug", "platform": platform.platform(), "base_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(), "binary_sha256": hashlib.sha256(BINARY.read_bytes()).hexdigest(), "source_sha256": hashlib.sha256(b"".join(p.read_bytes() for p in sorted([*(ROOT/"crates/engine/src").glob("*.rs"), *(ROOT/"crates/cluster/src").rglob("*.rs")]))).hexdigest(), "results": results, "throughput_ratio_2_vs_1": results[1]["requests_per_second"]/results[0]["requests_per_second"], "scope": "Single-host Query scaling only; writer still retains full collection state. RSS is a post-run sample, not peak RSS; no production throughput claim."}
    report["logical_cpus"] = os.cpu_count()
    report["rustc"] = subprocess.check_output(["rustc", "--version"], text=True).strip()
    if platform.system() == "Darwin":
        report["hardware"] = subprocess.check_output(["sysctl", "-n", "machdep.cpu.brand_string"], text=True).strip()
        report["host_memory_bytes"] = int(subprocess.check_output(["sysctl", "-n", "hw.memsize"], text=True).strip())
    (ROOT/"docs/benchmarks/m4-baseline.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2), flush=True)
    for p in [writer, qa, qb]:
        stop(p)


def main():
    global TMP
    parser = argparse.ArgumentParser()
    parser.add_argument("--benchmark", action="store_true")
    args = parser.parse_args()
    command("exec", "-T", "minio", "mcli", "alias", "set", "test", "http://localhost:9000", "development", "development-only-password")
    command("exec", "-T", "minio", "mcli", "mb", "--ignore-existing", "test/vectordb")
    with tempfile.TemporaryDirectory(prefix="vectordb-m4-") as TMP:
        try:
            benchmark() if args.benchmark else acceptance()
        except Exception:
            for path in LOGS:
                print(f"--- {path.name} ---\n{path.read_text()[-8000:]}", flush=True)
            raise
        finally:
            for p in PROCESSES:
                stop(p)


if __name__ == "__main__":
    main()
