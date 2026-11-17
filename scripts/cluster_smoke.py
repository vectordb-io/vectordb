#!/usr/bin/env python3
"""Real etcd/Kafka/S3 integration test; uses only a dedicated Compose project.

Start dependencies with the documented Compose command first. This test creates a
unique topic/generation and removes its processes; the caller owns dependency cleanup.
"""
import concurrent.futures
import json
import os
import signal
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
import uuid

ROOT = Path(__file__).resolve().parents[1]
COMPOSE = ["docker", "compose", "-p", "vectordb-cluster-test", "-f", str(ROOT / "deployments/cluster/compose.yaml")]
TOKEN = "development-integration-token"
KEY_ISSUED = int(time.time() * 1000)
GENERATION = "test" + uuid.uuid4().hex
ENV = {
    **os.environ,
    "VECTORDB_CLUSTER": "smoke",
    "VECTORDB_COLLECTION": "items",
    "VECTORDB_GENERATION": GENERATION,
    "VECTORDB_ETCD": "http://127.0.0.1:12379",
    "VECTORDB_KAFKA": "127.0.0.1:19092",
    "VECTORDB_S3_ENDPOINT": "http://127.0.0.1:19000",
    "VECTORDB_S3_ACCESS_KEY": "development",
    "VECTORDB_S3_SECRET_KEY": "development-only-password",
    "VECTORDB_CLUSTER_TOKEN": TOKEN,
}
BINARY = ROOT / "target/debug/vectordb-cluster"


def command(*args):
    return subprocess.run([*COMPOSE, *args], check=True, text=True, capture_output=True, timeout=120)


def api(port, path, body=None, key=None, token=TOKEN):
    headers = {"Authorization": f"Bearer {token}"}
    data = None
    if body is not None:
        data = json.dumps(body).encode()
        headers["Content-Type"] = "application/json"
    if key:
        headers["Idempotency-Key"] = f"v2-{KEY_ISSUED}-{key}"
    req = urllib.request.Request(f"http://127.0.0.1:{port}{path}", data=data, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=50) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as error:
        return error.code, json.load(error)


def wait_ready(port, process):
    deadline = time.monotonic() + 90
    last = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"process exited with {process.returncode}")
        try:
            status, response = api(port, "/readyz")
            if status == 200:
                return
            last = response
        except (OSError, ValueError) as error:
            last = error
        time.sleep(0.3)
    raise RuntimeError(f"process not ready: {last}")


def port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def main():
    topic = f"vdb.smoke.items.{GENERATION}"
    deadline = time.monotonic() + 180
    while True:
        try:
            command("exec", "-T", "kafka", "/opt/kafka/bin/kafka-topics.sh", "--bootstrap-server", "kafka:9094", "--create", "--if-not-exists", "--topic", topic, "--partitions", "1", "--replication-factor", "1", "--config", "retention.ms=-1", "--config", "retention.bytes=-1", "--config", "max.message.bytes=2097152")
            command("exec", "-T", "minio", "mcli", "alias", "set", "test", "http://localhost:9000", "development", "development-only-password")
            command("exec", "-T", "minio", "mcli", "mb", "--ignore-existing", "test/vectordb")
            break
        except subprocess.CalledProcessError as error:
            if time.monotonic() > deadline:
                raise RuntimeError(error.stderr) from error
            time.sleep(2)
    processes = []
    with tempfile.TemporaryDirectory(prefix="vectordb-cluster-") as directory:
        logs = []

        def start(role, listen=None):
            listen = listen or port()
            log = open(Path(directory) / f"{role}-{len(logs)}.log", "w+")
            logs.append(log)
            args = [str(BINARY), "--role", role, "--bind", f"127.0.0.1:{listen}"]
            if role == "writer":
                args += ["--advertise", f"http://127.0.0.1:{listen}"]
            process = subprocess.Popen(args, env=ENV, stdout=log, stderr=log)
            processes.append(process)
            wait_ready(listen, process)
            return process, listen

        def stop(process, kill=False):
            if process.poll() is None:
                (process.kill if kill else process.terminate)()
                process.wait(timeout=30)

        def request(listen, operation, body, key=None):
            status, result = api(listen, "/v2/vectordb/" + operation, body, key)
            assert status == 200 and result["code"] == 0, (status, result)
            return result

        try:
            print("Starting writer and two independent query processes", flush=True)
            writer, wp = start("writer")
            q1, qp1 = start("query")
            q2, qp2 = start("query")
            request(qp1, "collections/create", {"collectionName": "items", "dimension": 2, "autoID": True}, "create")
            body = {"collectionName": "items", "data": [{"vector": [1, 0]}, {"vector": [0, 1]}]}
            accepted = request(qp1, "entities/insert", body, "first")
            assert accepted["data"]["insertIds"] == [1, 2]
            assert request(qp2, "entities/insert", body, "first") == accepted
            status, _ = api(qp1, "/v2/vectordb/entities/insert", {"collectionName": "items", "data": [{"vector": [9, 9]}]}, "first")
            assert status == 409
            status, _ = api(qp1, "/v2/vectordb/entities/insert", body)
            assert status == 400
            status, _ = api(qp1, "/readyz", token="wrong")
            assert status == 401
            bad = {"collectionName": "items", "data": [{"vector": [1, 1]}, {"vector": [1]}]}
            status, _ = api(qp1, "/v2/vectordb/entities/insert", bad, "invalid-batch")
            assert status == 400
            print("Checking concurrent writes and Strong visibility on both replicas", flush=True)
            def insert(i):
                return request(qp1 if i % 2 else qp2, "entities/insert", {"collectionName": "items", "data": [{"vector": [i, 1]}]}, f"concurrent-{i}")
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
                results = list(executor.map(insert, range(8)))
            ids = [r["data"]["insertIds"][0] for r in results]
            assert sorted(ids) == list(range(3, 11)), ids
            for qp in [qp1, qp2]:
                assert request(qp, "collections/get_stats", {"collectionName": "items"})["data"]["rowCount"] == 10
            search = {"collectionName": "items", "data": [[1, 0]], "limit": 3}
            expected = request(qp1, "entities/search", search)["data"]
            assert request(qp2, "entities/search", search)["data"] == expected
            request(qp1, "entities/delete", {"collectionName": "items", "ids": [2]}, "delete-two")
            request(qp1, "entities/upsert", {"collectionName": "items", "data": [{"id": 1, "vector": [3, 4]}]}, "update-one")
            print("Publishing and verifying an S3 checkpoint", flush=True)
            completed = subprocess.run([str(BINARY), "--role", "checkpoint"], env=ENV, check=True, capture_output=True, text=True, timeout=90)
            manifest = json.loads(completed.stdout)["manifest"]
            assert manifest["offset"] >= accepted["commitToken"]["offset"]
            # This acknowledged mutation is deliberately newer than the checkpoint.
            tail = {"collectionName": "items", "data": [{"vector": [2, 2]}]}
            tail_result = request(qp1, "entities/insert", tail, "tail")
            print("SIGKILL all compute processes; recover checkpoint plus WAL tail", flush=True)
            for process in [writer, q1, q2]:
                stop(process, kill=True)
            # Delete only this test topic's prefix, already covered by the verified
            # checkpoint. Recovery must use S3 rather than replay Kafka from zero.
            deletion = json.dumps({"partitions": [{"topic": topic, "partition": 0, "offset": manifest["offset"] + 1}], "version": 1})
            subprocess.run([*COMPOSE, "exec", "-T", "kafka", "sh", "-c", "cat > /tmp/vectordb-smoke-delete.json"], input=deletion, text=True, check=True, timeout=20)
            command("exec", "-T", "kafka", "/opt/kafka/bin/kafka-delete-records.sh", "--bootstrap-server", "kafka:9094", "--offset-json-file", "/tmp/vectordb-smoke-delete.json")
            # Lease expiry is the explicit P1 crash recovery boundary.
            time.sleep(32)
            writer, wp = start("writer")
            q1, qp1 = start("query")
            assert request(qp1, "entities/insert", tail, "tail") == tail_result
            assert request(qp1, "entities/insert", body, "first") == accepted
            assert request(qp1, "collections/get_stats", {"collectionName": "items"})["data"]["rowCount"] == 10
            assert request(qp1, "entities/get", {"collectionName": "items", "id": 2})["data"] == []
            assert request(qp1, "entities/get", {"collectionName": "items", "id": 1, "outputFields": ["vector"]})["data"][0]["vector"] == [3, 4]
            next_row = request(wp, "entities/insert", tail, "after-restart")
            assert next_row["data"]["insertIds"] == [12]
            request(wp, "entities/delete", {"collectionName": "items", "ids": [12]}, "delete-twelve")
            print("Exercising automatic flush, WAL reclamation and lagging query reload", flush=True)
            for i in range(150):
                request(wp, "entities/upsert", {"collectionName": "items", "data": [{"id": 1, "vector": [i, 4]}]}, f"repeated-{i}")
            assert request(qp1, "entities/get", {"collectionName": "items", "id": 1, "outputFields": ["vector"]})["data"][0]["vector"] == [149, 4]
            # The old manifest namespace must be reclaimed after its pin is released.
            old_stat = subprocess.run([*COMPOSE, "exec", "-T", "minio", "mcli", "stat", "test/vectordb/" + manifest["key"]], capture_output=True, timeout=20)
            assert old_stat.returncode != 0, "retired checkpoint was not reclaimed"
            print("Checking that an active writer cannot be replaced", flush=True)
            duplicate = subprocess.run([str(BINARY), "--role", "writer", "--advertise", "http://127.0.0.1:1"], env=ENV, capture_output=True, text=True, timeout=20)
            assert duplicate.returncode != 0
            assert "already has a writer" in duplicate.stderr
            print("Pausing old writer across lease expiry and fencing it after takeover", flush=True)
            old_writer, old_port = writer, wp
            os.kill(old_writer.pid, signal.SIGSTOP)
            try:
                time.sleep(32)
                writer, wp = start("writer")
            finally:
                os.kill(old_writer.pid, signal.SIGCONT)
            status, _ = api(old_port, "/v2/vectordb/entities/insert", tail, "stale-owner")
            assert status == 503
            assert request(qp1, "collections/get_stats", {"collectionName": "items"})["data"]["rowCount"] == 10
            stop(old_writer)
            # No dependency = no successful Strong read or acknowledged write.
            command("stop", "kafka")
            try:
                status, result = api(qp1, "/v2/vectordb/entities/insert", tail, "during-outage")
                assert status == 503, (status, result)
            finally:
                command("start", "kafka")
            print("PASS: atomic batches, dedup, deletes, native checkpoints, automatic flush/GC, lagging reload, SIGKILL recovery, fencing, Kafka outage", flush=True)
        except BaseException:
            for log in logs:
                log.flush()
                log.seek(0)
                print(log.read()[-12000:])
            raise
        finally:
            for process in processes:
                stop(process)
            for log in logs:
                log.close()
            # Remove only the unique test log; never touch user topics.
            try:
                command("exec", "-T", "kafka", "/opt/kafka/bin/kafka-topics.sh", "--bootstrap-server", "kafka:9094", "--delete", "--topic", topic)
            except (subprocess.SubprocessError, OSError):
                pass


if __name__ == "__main__":
    main()
