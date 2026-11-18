#!/usr/bin/env python3
"""Exercise M6 against the isolated Compose backends using fresh logical clusters."""

from contextlib import contextmanager
import hashlib
import os
import json
from pathlib import Path
import subprocess
import tempfile
import time
import uuid

import cluster_m3_smoke as gateway
from cluster_smoke import BINARY, ENV, ROOT, api, port
import sys

sys.path.insert(0, str(ROOT / "python/vectordb_io"))
from cluster import ClusterClient, ClusterError

READ = "integration-read-only-token"
WRITE = "integration-read-write-token"


def admin(role, cluster, archive, *args, success=True, crash=None):
    result = subprocess.run(
        [str(BINARY), "--role", role, "--archive", str(archive), *args],
        env={**ENV, "VECTORDB_CLUSTER": cluster, "VECTORDB_TEST_CRASH_AT": crash or ""},
        text=True,
        capture_output=True,
        timeout=120,
    )
    assert (result.returncode == 0) == success, (result.stdout, result.stderr)
    return result


def name():
    return "m6-" + uuid.uuid4().hex


def sqlite_restore(tmp):

    # Build the fixture through the actual SQLite HTTP entry point.
    from cluster_smoke import port

    address = port()
    db = tmp / "old.sqlite"
    server = subprocess.Popen(
        [
            str(ROOT / "target/debug/vectordb"),
            "serve",
            "--bind",
            f"127.0.0.1:{address}",
            "--data",
            str(db),
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        for _ in range(100):
            try:
                if api(address, "/healthz")[0] == 200:
                    break
            except OSError:
                time.sleep(0.1)
        base = {"collectionName": "legacy"}

        def old(op, **kwargs):
            return gateway.call(address, op, {**base, **kwargs})["data"]

        old("collections/create", dimension=2, autoID=True)
        old("aliases/create", aliasName="oldalias")
        old("partitions/create", partitionName="p")
        old(
            "entities/insert",
            partitionName="p",
            data=[{"vector": [1.123456789, 0], "flag": True}, {"vector": [2, 0]}],
        )
        old("entities/delete", filter="id == 2")
        query = {"filter": "id >= 1", "outputFields": ["id", "vector", "flag"]}
        expected = old("entities/query", **query)
        search = {"data": [[1, 0]], "limit": 1}
        ranked = old("entities/search", **search)
    finally:
        server.terminate()
        server.wait(timeout=10)
    archive = tmp / "sqlite-backup"
    snapshot = tmp / "migration.sqlite"
    subprocess.run(
        [
            str(ROOT / "target/debug/vectordb"),
            "backup",
            "--data",
            str(db),
            "--output",
            str(snapshot),
        ],
        check=True,
    )
    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts/migrate_sqlite.py"),
            str(snapshot),
            str(archive),
        ],
        check=True,
    )
    dest = name()
    admin("restore", dest, archive)
    process, address = gateway.start(dest)
    try:
        assert (
            gateway.call(
                address, "entities/query", {"collectionName": "oldalias", **query}
            )["data"]
            == expected
        )
        assert (
            gateway.call(
                address, "entities/search", {"collectionName": "oldalias", **search}
            )["data"]
            == ranked
        )
        assert gateway.call(
            address,
            "entities/insert",
            {**base, "data": [{"vector": [0, 1]}]},
            "sqlite-next",
        )["data"]["insertIds"] == [3]
    finally:
        gateway.stop(process)


def native_parity(tmp, remote):
    address = port()
    process = subprocess.Popen(
        [
            str(ROOT / "target/debug/vectordb"),
            "serve",
            "--bind",
            f"127.0.0.1:{address}",
            "--data",
            str(tmp / "native.vdb"),
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        for _ in range(100):
            try:
                if api(address, "/healthz")[0] == 200:
                    break
            except OSError:
                time.sleep(0.1)
        base = {"collectionName": "parity"}
        for op, fields in [
            (
                "collections/create",
                {"dimension": 2, "metricType": "L2", "shardCount": 4},
            ),
            ("partitions/create", {"partitionName": "p"}),
            (
                "entities/insert",
                {
                    "partitionName": "p",
                    "data": [
                        {"id": 1, "vector": [0.2, 0.3]},
                        {"id": 2, "vector": [1, 1]},
                    ],
                },
            ),
            (
                "entities/upsert",
                {"partitionName": "p", "data": [{"id": 1, "vector": [0.3, 0.2]}]},
            ),
            ("entities/delete", {"ids": [2]}),
            ("collections/flush", {}),
            ("entities/count", {}),
            ("entities/query", {"filter": "id >= 0", "outputFields": ["id", "vector"]}),
            ("entities/get", {"id": [1, 2], "outputFields": ["id", "vector"]}),
            ("entities/search", {"data": [[0.2, 0], [0, 0.3]], "limit": 2}),
        ]:
            body = {**base, **fields}
            single = gateway.call(address, op, body)["data"]
            distributed = gateway.call(
                remote, op, body, "parity-" + op.replace("/", "-")
            )["data"]
            if op.startswith("entities/"):
                assert single == distributed, (op, single, distributed)
    finally:
        process.terminate()
        process.wait(timeout=10)


def p1_restore(tmp):
    scope = dict(cluster="oldp1", collection="legacy", generation="original")

    def compact(value):
        return json.dumps(value, separators=(",", ":"), sort_keys=True).encode()

    history = []

    def append(entry):
        history.append(
            dict(
                offset=len(history) * 2, record=dict(format=1, scope=scope, entry=entry)
            )
        )

    append(dict(kind="activate", epoch=10))
    legacy_id = ClusterClient.new_key()
    for key, op, body, result in [
        (
            "create",
            "collections/create",
            {"collectionName": "legacy", "dimension": 2},
            {},
        ),
        (
            legacy_id,
            "entities/insert",
            {"collectionName": "legacy", "data": [{"id": 7, "vector": [0, 1]}]},
            {"insertCount": 1, "insertIds": [7]},
        ),
    ]:
        append(
            dict(
                kind="mutation",
                epoch=10,
                operation=op,
                request=body,
                result=result,
                request_id=key,
                digest=hashlib.sha256(compact([op, body])).hexdigest(),
            )
        )
    append(dict(kind="barrier", epoch=10))
    data = compact(dict(format=1, scope=scope, offset=6, records=history))
    cp = tmp / "p1.json"
    cp.write_bytes(data)
    m = tmp / "p1-manifest.json"
    m.write_bytes(
        compact(
            dict(
                scope=scope,
                offset=6,
                key="oldp1/legacy/original/checkpoints/cp.json",
                sha256=hashlib.sha256(data).hexdigest(),
                bytes=len(data),
            )
        )
    )
    db = tmp / "p1.sqlite"
    export = [
        str(BINARY.with_name("vectordb-p1-export")),
        "--manifest",
        str(m),
        "--checkpoint",
        str(cp),
        "--output",
        str(db),
    ]
    cp.write_bytes(data + b"x")
    assert subprocess.run(export, capture_output=True).returncode and not db.exists()
    cp.write_bytes(data)
    subprocess.run(export, check=True)
    target = name()
    archive = tmp / "p1-backup"
    subprocess.run(
        [
            sys.executable,
            str(ROOT / "scripts/migrate_sqlite.py"),
            str(db),
            str(archive),
        ],
        check=True,
    )
    admin("restore", target, archive)
    p, addr = gateway.start(target)
    try:
        client = ClusterClient(f"http://127.0.0.1:{addr}", WRITE)
        try:
            client.execute(
                "entities/insert",
                {"collectionName": "legacy", "data": [{"id": 8, "vector": [0, 1]}]},
                legacy_id,
            )
        except ClusterError as e:
            assert e.status == 409
        else:
            raise AssertionError("retired P1 key was reused")
        assert gateway.call(
            addr,
            "entities/get",
            {"collectionName": "legacy", "id": 7, "outputFields": ["id", "vector"]},
        )["data"] == [{"id": 7, "vector": [0, 1]}]
    finally:
        gateway.stop(p)


@contextmanager
def empty_backends():
    etcd, kafka, s3 = port(), port(), port()
    env = {
        **os.environ,
        "VDB_DEV_ETCD_PORT": str(etcd),
        "VDB_DEV_KAFKA_PORT": str(kafka),
        "VDB_DEV_S3_PORT": str(s3),
    }
    cmd = [
        "docker",
        "compose",
        "-p",
        "vdb-m6-restore-" + uuid.uuid4().hex[:8],
        "-f",
        str(ROOT / "deployments/cluster/compose.yaml"),
    ]
    try:
        subprocess.run(
            cmd + ["up", "-d"], check=True, env=env, capture_output=True, timeout=120
        )
        until = time.monotonic() + 60
        while True:
            ready = subprocess.run(
                cmd
                + [
                    "exec",
                    "-T",
                    "minio",
                    "mcli",
                    "alias",
                    "set",
                    "dest",
                    "http://localhost:9000",
                    "development",
                    "development-only-password",
                ],
                env=env,
                capture_output=True,
            )
            if ready.returncode == 0:
                break
            assert time.monotonic() < until, ready.stderr
            time.sleep(0.4)
        subprocess.run(
            cmd + ["exec", "-T", "minio", "mcli", "mb", "dest/vectordb"],
            check=True,
            env=env,
            capture_output=True,
        )
        yield {
            "VECTORDB_ETCD": f"http://127.0.0.1:{etcd}",
            "VECTORDB_KAFKA": f"127.0.0.1:{kafka}",
            "VECTORDB_S3_ENDPOINT": f"http://127.0.0.1:{s3}",
        }
    finally:
        subprocess.run(
            cmd + ["down", "-v"], env=env, check=True, capture_output=True, timeout=90
        )


def crash_at_publication():
    import concurrent.futures

    def one(point):
        cluster = name()
        p, address = gateway.start(cluster, point)
        base = {"collectionName": "items"}
        gateway.call(address, "collections/create", {**base, "dimension": 2}, "create")
        body = {**base, "data": [{"id": 7, "vector": [1, 2]}]}
        original = gateway.call(address, "entities/insert", body, "insert")
        try:
            api(address, "/v2/vectordb/collections/flush", base)
        except OSError:
            pass
        p.wait(timeout=40)
        assert p.returncode != 0, point
        p, recovered = gateway.start(cluster)
        try:
            assert (
                gateway.retry(recovered, "entities/count", base)["data"]["count"] == 1
            )
            assert (
                gateway.call(recovered, "entities/insert", body, "insert") == original
            )
        finally:
            gateway.stop(p)

    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as workers:
        list(
            workers.map(
                one, ["checkpoint_uploaded", "checkpoint_published", "wal_trimmed"]
            )
        )


def exercise(tmp, destination):
    src, dest = name(), name()
    gateway.ENV.update(
        VECTORDB_READ_TOKEN=READ,
        VECTORDB_WRITE_TOKEN=WRITE,
        VECTORDB_NAMESPACE_COLLECTIONS="2",
    )
    p, addr = gateway.start(src)
    origin = f"http://127.0.0.1:{addr}"
    clients = [
        ClusterClient(origin, token)
        for token in [READ, WRITE, ENV["VECTORDB_CLUSTER_TOKEN"]]
    ]
    reader, writer, owner = clients
    base = {"collectionName": "items"}

    def denied(client, op, body, status):
        try:
            client.execute(op, body, client.new_key())
        except ClusterError as e:
            assert e.status == status, (e.status, e)
        else:
            raise AssertionError(op)

    denied(reader, "entities/insert", base, 403)
    denied(writer, "collections/create", base, 403)
    denied(reader, "indexes/build", base, 403)
    denied(writer, "collections/compact", base, 403)
    owner.execute(
        "collections/create", {**base, "dimension": 2, "autoID": True}, owner.new_key()
    )
    owner.execute(
        "collections/create",
        {"collectionName": "second", "dimension": 2},
        owner.new_key(),
    )
    denied(
        owner, "collections/create", {"collectionName": "overflow", "dimension": 2}, 409
    )
    dropped = {"collectionName": "second", "data": [{"id": 9, "vector": [0, 2]}]}
    old_key = writer.new_key()
    old = writer.execute("entities/insert", dropped, old_key)
    owner.execute("collections/drop", {"collectionName": "second"}, owner.new_key())
    owner.execute("aliases/create", {**base, "aliasName": "current"}, owner.new_key())
    body = {
        "collectionName": "current",
        "data": [{"vector": [1, 0]}, {"vector": [0, 1]}],
    }
    key = writer.new_key()
    accepted = writer.execute("entities/insert", body, key)
    writer.execute("entities/delete", {**base, "ids": [2]}, writer.new_key())
    assert reader.execute("entities/count", base)["data"]["count"] == 1
    filter_query = {**base, "filter": "id >= 1", "outputFields": ["id", "vector"]}
    expected = reader.execute("entities/query", filter_query)["data"]
    archive = tmp / "backup"
    admin("backup", src, archive, "--gateway", origin)
    writer.execute(
        "entities/upsert",
        {**base, "data": [{"id": 5, "vector": [2, 2]}]},
        writer.new_key(),
    )
    gateway.stop(p)
    index = json.loads((archive / "catalog.json").read_bytes())
    file = archive / (next(iter(index["collections"])) + ".json")
    original = file.read_bytes()
    file.write_bytes(original + b"!")
    admin("restore", dest, archive, success=False)
    file.write_bytes(original)
    assert hashlib.sha256(original).hexdigest() in index["collections"].values()
    admin("restore", src, archive, success=False)
    # Move the restore tool and all subsequent gateways to distinct empty etcd/Kafka/S3 volumes.
    ENV.update(destination)
    crashed = admin("restore", dest, archive, success=False, crash="restore_staged")
    assert "test crash at restore_staged" in crashed.stderr
    verifier, verify_addr = gateway.start(dest)
    assert gateway.call(verify_addr, "collections/list", {})["data"] == []
    gateway.stop(verifier)
    until = time.monotonic() + 55
    while True:
        result = subprocess.run(
            [str(BINARY), "--role", "restore", "--archive", str(archive)],
            env={**ENV, "VECTORDB_CLUSTER": dest},
            capture_output=True,
            text=True,
            timeout=90,
        )
        if result.returncode == 0:
            break
        assert "already has a writer" in result.stderr and time.monotonic() < until, (
            result.stderr
        )
        time.sleep(0.5)
    admin("restore", dest, archive, success=False)
    p, target = gateway.start(dest)
    client = ClusterClient(f"http://127.0.0.1:{target}", WRITE)
    assert client.execute("entities/query", filter_query)["data"] == expected
    retry = client.execute("entities/insert", body, key)
    assert (
        retry["data"] == accepted["data"]
        and retry["commitToken"]["scope"]["cluster"] == dest
    )
    assert client.execute("entities/count", base)["data"]["count"] == 1
    assert (
        client.execute("collections/has", {"collectionName": "second"})["data"]["has"]
        is False
    )
    assert client.execute("entities/insert", dropped, old_key)["data"] == old["data"]
    assert client.execute(
        "entities/insert", {**base, "data": [{"vector": [1, 1]}]}, client.new_key()
    )["data"]["insertIds"] == [3]
    native_parity(tmp, target)
    gateway.stop(p)
    gateway.ENV["VECTORDB_REQUESTS_PER_SECOND"] = "1"
    p, limited = gateway.start(name())
    limiter = ClusterClient(f"http://127.0.0.1:{limited}", READ)
    limiter.execute("databases/list", {})
    denied(limiter, "databases/list", {}, 429)
    gateway.stop(p)
    del gateway.ENV["VECTORDB_REQUESTS_PER_SECOND"]
    sqlite_restore(tmp)
    p1_restore(tmp)
    crash_at_publication()
    print(
        "M6: verified archive digests, independent restore, ACL/rate/quota, dedup, aliases, SQLite query parity and next IDs",
        flush=True,
    )


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="vectordb-m6-") as folder:
        gateway.TMP = folder
        try:
            with empty_backends() as target:
                exercise(Path(folder), target)
        except BaseException:
            for log in gateway.LOGS:
                print(log, log.read_text()[-1500:], file=sys.stderr)
            raise
        finally:
            for process in gateway.PROCESSES:
                gateway.stop(process)
