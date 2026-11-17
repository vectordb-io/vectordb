#!/usr/bin/env python3
"""M3 catalog/routing/recovery acceptance against isolated real dependencies."""
import concurrent.futures
import os
import signal
import subprocess
import tempfile
import time
import uuid
from pathlib import Path
from cluster_smoke import BINARY, ENV, api, command, port, wait_ready

PROCESSES = []
LOGS = []
TMP = None


def start(cluster, crash=None):
    address = port()
    env = {**ENV, "VECTORDB_CLUSTER": cluster}
    env.pop("VECTORDB_TEST_CRASH_AT", None)
    if crash:
        env["VECTORDB_TEST_CRASH_AT"] = crash
    path = Path(TMP) / f"{cluster}-{address}.log"
    stream = path.open("w")
    process = subprocess.Popen([str(BINARY), "--role", "node", "--bind", f"127.0.0.1:{address}", "--advertise", f"http://127.0.0.1:{address}"], env=env, stdout=stream, stderr=stream)
    stream.close()
    PROCESSES.append(process)
    LOGS.append(path)
    wait_ready(address, process)
    return process, address


def stop(process):
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=10)


def call(address, op, body, key=None, expected=200):
    status, result = api(address, "/v2/vectordb/" + op, body, key)
    assert status == expected, (op, status, result)
    return result


def retry(address, op, body, key=None):
    deadline = time.monotonic() + 85
    while True:
        try:
            status, result = api(address, "/v2/vectordb/" + op, body, key)
            if status == 200:
                return result
            assert status in (409, 503), (op, status, result)
        except OSError:
            pass
        if time.monotonic() > deadline:
            raise AssertionError(f"recovery deadline: {op}")
        time.sleep(0.5)


def basic():
    cluster = "m3-" + uuid.uuid4().hex
    first, a = start(cluster)
    second, b = start(cluster)
    capabilities = api(a, "/capabilities")[1]
    assert capabilities["mode"] == "cluster-m3"
    assert "entities/count" in capabilities["operations"]
    assert "collections/rename" not in capabilities["operations"]
    assert len(capabilities["operations"]) == len(set(capabilities["operations"]))
    print("M3: namespaces, independent collections, aliases and atomic batches", flush=True)
    call(a, "databases/create", {"dbName": "other"}, "namespace")
    create = {"collectionName": "items", "dimension": 2, "autoID": True}
    c1 = call(a, "collections/create", create, "create")
    c2 = call(b, "collections/create", {**create, "collectionName": "second"}, "create2")
    call(a, "collections/create", {**create, "dbName": "other"}, "other")
    assert c1["data"] != c2["data"]
    # Controls must catch up a newly opened query replica before reading its schema.
    call(b, "indexes/get_status", {"collectionName": "items"})
    call(b, "indexes/drop", {"collectionName": "items", "indexName": "vector"}, "drop-index")
    call(b, "indexes/create", {"collectionName": "items", "indexParams": [{"fieldName": "vector", "indexName": "graph", "indexType": "HNSW", "metricType": "COSINE", "params": {"M": 8, "efConstruction": 32}}]}, "create-index")
    assert call(a, "indexes/describe", {"collectionName": "items", "indexName": "graph"})["data"][0]["indexType"] == "HNSW"
    assert call(b, "indexes/cancel", {"collectionName": "items"})["data"]["paused"] is True
    assert call(b, "indexes/retry", {"collectionName": "items"})["data"]["paused"] is False
    call(b, "collections/flush", {"dbName": "other", "collectionName": "items"})

    call(b, "aliases/create", {"collectionName": "items", "aliasName": "live"}, "alias")
    body = {"collectionName": "live", "data": [{"vector": [1, 0]}]}
    inserted = call(b, "entities/insert", body, "entity")
    assert inserted["data"]["insertIds"] == [1]
    call(a, "entities/insert", {"collectionName": "items", "data": [{"vector": [1, 0]}, {"vector": [0]}]}, "invalid-batch", 400)
    assert call(a, "entities/count", {"collectionName": "items"})["data"]["count"] == 1
    result = call(a, "entities/insert", {"collectionName": "items", "data": [{"vector": [0, 1]}]}, "next")
    assert result["data"]["insertIds"] == [2]
    call(a, "aliases/alter", {"collectionName": "second", "aliasName": "live"}, "switch")
    assert call(b, "entities/insert", body, "entity") == inserted
    assert call(b, "entities/count", {"collectionName": "live"})["data"]["count"] == 0
    assert call(b, "entities/count", {"collectionName": "items", "dbName": "other"})["data"]["count"] == 0
    call(b, "entities/insert", body, "entity-new")
    call(b, "entities/upsert", {"collectionName": "items", "data": [{"id": 1, "vector": [0.5, 0.5]}]}, "upsert")
    call(a, "entities/delete", {"collectionName": "items", "ids": [2]}, "delete")
    call(a, "partitions/create", {"collectionName": "items", "partitionName": "p"}, "p")
    call(b, "entities/insert", {"collectionName": "items", "partitionName": "p", "data": [{"vector": [1, 0]}, {"vector": [0, 1]}]}, "p-data")
    call(b, "entities/delete", {"collectionName": "items", "ids": [4]}, "p-delete")
    call(b, "partitions/drop", {"collectionName": "items", "partitionNames": ["p", "missing"]}, "p-bad", 404)
    assert call(a, "entities/count", {"collectionName": "items"})["data"]["count"] == 2
    call(b, "partitions/drop", {"collectionName": "items", "partitionNames": ["p"]}, "p-drop")
    call(a, "collections/flush", {"collectionName": "items"})
    print("M3: concurrent reads, owner takeover, checkpoint and WAL recovery", flush=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        futures = [pool.submit(call, b, "entities/search", {"collectionName": "items", "data": [[1, 0]], "limit": 10}) for _ in range(8)]
        for future in futures:
            assert [row["id"] for row in future.result()["data"]] == [1]
    stop(first)
    assert retry(b, "entities/count", {"collectionName": "items"})["data"]["count"] == 1
    assert call(b, "entities/insert", body, "entity") == inserted
    print("M3: drop/recreate isolation, old retries and alias removal", flush=True)
    call(b, "collections/drop", {"collectionName": "items"}, "drop")
    assert call(b, "collections/has", {"collectionName": "items"})["data"]["has"] is False
    recreated = call(b, "collections/create", create, "recreate")
    assert recreated["data"] != c1["data"]
    assert call(b, "entities/count", {"collectionName": "items"})["data"]["count"] == 0
    # Even after same-name recreation, a previously bound write returns the old result.
    assert call(b, "entities/insert", body, "entity") == inserted
    assert call(b, "collections/create", create, "create") == c1
    assert call(b, "entities/count", {"collectionName": "items"})["data"]["count"] == 0
    call(b, "collections/drop", {"collectionName": "second"}, "drop-second")
    call(b, "aliases/describe", {"aliasName": "live"}, expected=404)
    call(b, "databases/drop", {"dbName": "other"}, "nonempty", 409)
    call(b, "collections/drop", {"dbName": "other", "collectionName": "items"}, "drop-other")
    call(b, "databases/drop", {"dbName": "other"}, "drop-db")
    call(b, "collections/rename", {"collectionName": "items", "newCollectionName": "x"}, "unsupported", 501)
    stop(second)


def crash_case(point):
    print(f"M3: crash recovery at {point}", flush=True)
    cluster = "m3-" + uuid.uuid4().hex
    create = {"collectionName": "items", "dimension": 2, "autoID": True}
    is_drop = point in ("catalog_dropping", "drop_committed", "catalog_dropped")
    is_write = point == "mutation_committed"
    if is_drop or is_write:
        process, address = start(cluster)
        created = call(address, "collections/create", create, "create")
        call(address, "entities/insert", {"collectionName": "items", "data": [{"vector": [1, 0]}]}, "initial")
        stop(process)
    process, address = start(cluster, point)
    op, body, key = "collections/create", create, "create"
    if is_drop:
        op, body, key = "collections/drop", {"collectionName": "items"}, "drop"
    if is_write:
        op, body, key = "entities/insert", {"collectionName": "items", "data": [{"vector": [0, 1]}]}, "uncertain"
    try:
        api(address, "/v2/vectordb/" + op, body, key)
    except OSError:
        pass
    process.wait(timeout=60)
    assert process.returncode != 0, point
    assert f"test crash at {point}" in LOGS[-1].read_text(), point
    clean, recovered = start(cluster)
    result = retry(recovered, op, body, key)
    if is_drop:
        assert call(recovered, "collections/has", {"collectionName": "items"})["data"]["has"] is False
        new = call(recovered, "collections/create", create, "recreate")
        assert new["data"] != created["data"]
        assert call(recovered, "entities/count", {"collectionName": "items"})["data"]["count"] == 0
    elif is_write:
        assert result["data"]["insertIds"] == [2]
        assert call(recovered, "entities/count", {"collectionName": "items"})["data"]["count"] == 2
        assert call(recovered, op, body, key) == result
    else:
        assert retry(recovered, "entities/count", {"collectionName": "items"})["data"]["count"] == 0
        assert call(recovered, op, body, key) == result
    stop(clean)


def uncertain_without_restart():
    print("M3: unknown commit outcome recovers in the same node process", flush=True)
    cluster = "m3-" + uuid.uuid4().hex
    process, address = start(cluster)
    call(address, "collections/create", {"collectionName": "items", "dimension": 2, "autoID": True}, "create")
    stop(process)
    process, address = start(cluster, "mutation_unknown")
    body = {"collectionName": "items", "data": [{"vector": [1, 0]}]}
    call(address, "entities/insert", body, "unknown", 503)
    result = retry(address, "entities/insert", body, "unknown")
    assert process.poll() is None
    assert result["data"]["insertIds"] == [1]
    assert call(address, "entities/count", {"collectionName": "items"})["data"]["count"] == 1
    stop(process)


def bound_retry():
    print("M3: crash after route binding, then switch alias before retry", flush=True)
    cluster = "m3-" + uuid.uuid4().hex
    process, address = start(cluster)
    for name in ["a", "b"]:
        call(address, "collections/create", {"collectionName": name, "dimension": 2, "autoID": True}, name)
    call(address, "aliases/create", {"collectionName": "a", "aliasName": "live"}, "alias")
    stop(process)
    process, address = start(cluster, "route_bound")
    body = {"collectionName": "live", "data": [{"vector": [1, 0]}]}
    try:
        api(address, "/v2/vectordb/entities/insert", body, "bound")
    except OSError:
        pass
    process.wait(timeout=60)
    assert "test crash at route_bound" in LOGS[-1].read_text()
    clean, address = start(cluster)
    call(address, "aliases/alter", {"collectionName": "b", "aliasName": "live"}, "switch")
    call(address, "entities/insert", body, "bound")
    assert call(address, "entities/count", {"collectionName": "a"})["data"]["count"] == 1
    assert call(address, "entities/count", {"collectionName": "b"})["data"]["count"] == 0
    stop(clean)


def main():
    global TMP
    deadline = time.monotonic() + 120
    while True:
        try:
            command("exec", "-T", "minio", "mcli", "alias", "set", "m3", "http://localhost:9000", "development", "development-only-password")
            command("exec", "-T", "minio", "mcli", "mb", "--ignore-existing", "m3/vectordb")
            command("exec", "-T", "kafka", "/opt/kafka/bin/kafka-topics.sh", "--bootstrap-server", "kafka:9094", "--list")
            break
        except subprocess.CalledProcessError:
            if time.monotonic() > deadline:
                raise
            time.sleep(1)
    with tempfile.TemporaryDirectory(prefix="vectordb-m3-") as TMP:
        try:
            basic()
            uncertain_without_restart()
            if not os.getenv("VECTORDB_M3_BASIC_ONLY"):
                for point in ["catalog_creating", "owner_assigned", "owner_recovering", "owner_activated", "owner_ready", "create_committed", "catalog_active", "catalog_dropping", "drop_committed", "catalog_dropped", "mutation_committed"]:
                    crash_case(point)
                bound_retry()
            print("PASS: M3 catalog, aliases, generation isolation, atomic batches, ordered deletes, takeover and crash recovery", flush=True)
        except Exception:
            for path in LOGS:
                print(f"--- {path.name} ---\n{path.read_text()[-10000:]}", flush=True)
            raise
        finally:
            for process in PROCESSES:
                stop(process)


if __name__ == "__main__":
    main()
