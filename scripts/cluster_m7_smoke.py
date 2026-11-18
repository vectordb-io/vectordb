#!/usr/bin/env python3
"""Exercise cross-channel decisions, barriers and writer takeover on real local backends."""

import concurrent.futures
from contextlib import contextmanager
import tempfile
import uuid

import cluster_m3_smoke as nodes
from cluster_smoke import api

BASE = {"collectionName": "items"}
BODY = {**BASE, "data": [{"vector": [n, 1]} for n in range(8)]}


@contextmanager
def cluster():
    name = "m7-" + uuid.uuid4().hex
    processes = []

    def start(point=None):
        p, address = nodes.start(name, point)
        processes.append(p)
        return p, address

    try:
        yield start
    finally:
        for p in processes:
            nodes.stop(p)


def exercise():
    with cluster() as start:
        owner, a = start()
        _, b = start()
        nodes.retry(
            a,
            "collections/create",
            {
                **BASE,
                "dimension": 2,
                "autoID": True,
                "shardCount": 4,
                "writeChannels": 4,
            },
            "create",
        )
        nodes.call(a, "aliases/create", {**BASE, "aliasName": "live"}, "alias")
        replicas = [[f"http://127.0.0.1:{[a, b][i % 2]}"] for i in range(4)]
        nodes.call(
            a,
            "collections/set_query_routes",
            {**BASE, "expectedEpoch": 0, "replicas": replicas},
            "routes",
        )
        first = nodes.call(
            b, "entities/insert", {**BODY, "collectionName": "live"}, "first"
        )
        assert first["data"]["insertIds"] == list(range(1, 9))
        assert (
            nodes.call(
                a, "entities/insert", {**BODY, "collectionName": "live"}, "first"
            )
            == first
        )
        invalid = {
            **BASE,
            "data": [
                {"id": 50, "vector": [1, 2]},
                {"id": 1, "vector": [1, 2]},
                {"id": 50, "vector": [3, 4]},
            ],
        }
        nodes.call(b, "entities/upsert", invalid, "bad", 400)
        assert nodes.call(b, "entities/count", BASE)["data"]["count"] == 8
        print(
            "M7: two independent ingestion gateways, atomic interleaved writes and sharded Strong snapshots",
            flush=True,
        )

        def write(i):
            return nodes.retry([a, b][i % 2], "entities/insert", BODY, f"batch-{i}")

        def observe():
            while not done.is_set():
                result = nodes.retry(b, "entities/count", BASE)
                assert result["data"]["count"] % 8 == 0, result
                observed.append(
                    (result["readToken"]["offset"], result["data"]["count"])
                )

        import threading

        done = threading.Event()
        observed = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=5) as pool:
            observer = pool.submit(observe)
            try:
                commits = list(pool.map(write, range(40)))
            finally:
                done.set()
            observer.result()
        assert observed
        decision_offsets = [first["commitToken"]["offset"]] + [
            c["commitToken"]["offset"] for c in commits
        ]
        assert len(set(decision_offsets)) == 41
        assert all(
            count == 8 * sum(off <= barrier for off in decision_offsets)
            for barrier, count in observed
        )
        assert sorted(n for c in commits for n in c["data"]["insertIds"]) == list(
            range(9, 329)
        )
        nodes.call(a, "collections/flush", BASE)
        stats = nodes.call(a, "collections/storage_stats", BASE)["data"]
        assert len(stats["prepareWatermarks"]) == 4 and all(
            0 < lo <= hi for lo, hi in stats["prepareWatermarks"]
        ), stats
        nodes.call(
            b, "entities/delete", {**BASE, "ids": [1, 2, 3, 4, 5, 6, 7, 8]}, "delete"
        )
        assert nodes.call(a, "entities/count", BASE)["data"]["count"] == 320
        assert (
            nodes.call(
                b, "entities/insert", {**BODY, "collectionName": "live"}, "first"
            )
            == first
        )
        owner.kill()
        owner.wait(timeout=10)
        # Recover a checkpoint plus a multi-log tail on an empty compute process.
        _, recovered = start()
        # The test's static Query routes still contain the dead address.
        nodes.retry(
            recovered,
            "collections/set_query_routes",
            {
                **BASE,
                "expectedEpoch": 1,
                "replicas": [[f"http://127.0.0.1:{recovered}"]] * 4,
            },
            "replace",
        )
        assert nodes.retry(recovered, "entities/count", BASE)["data"]["count"] == 320
        assert (
            nodes.call(
                recovered,
                "entities/insert",
                {**BODY, "collectionName": "live"},
                "first",
            )
            == first
        )
        nodes.call(recovered, "partitions/create", {**BASE, "partitionName": "p"}, "p")
        accepted = nodes.call(
            recovered,
            "entities/insert",
            {**BODY, "partitionName": "p"},
            "partition-data",
        )
        nodes.call(
            recovered, "partitions/drop", {**BASE, "partitionNames": ["p"]}, "drop-p"
        )
        assert (
            nodes.call(
                recovered,
                "entities/insert",
                {**BODY, "partitionName": "p"},
                "partition-data",
            )
            == accepted
        )
        assert nodes.call(recovered, "entities/count", BASE)["data"]["count"] == 320


def crash(point):
    with cluster() as start:
        victim, a = start(point)
        nodes.retry(
            a,
            "collections/create",
            {**BASE, "dimension": 2, "writeChannels": 4, "autoID": True},
            "c",
        )
        try:
            if point in ("channel_cut", "checkpoint_published"):
                confirmed = nodes.call(a, "entities/insert", BODY, "batch")
                api(a, "/v2/vectordb/collections/flush", BASE)
            else:
                api(a, "/v2/vectordb/entities/insert", BODY, "batch")
        except OSError:
            pass
        victim.wait(timeout=30)
        assert victim.returncode != 0
        _, b = start()
        count = nodes.retry(b, "entities/count", BASE)["data"]["count"]
        assert count == (
            8
            if point in ("channel_decided", "channel_cut", "checkpoint_published")
            else 0
        ), (point, count)
        retried = nodes.retry(b, "entities/insert", BODY, "batch")
        if point in ("channel_cut", "checkpoint_published"):
            assert retried == confirmed
        assert retried["data"]["insertIds"] == list(range(1, 9))
        assert nodes.call(b, "entities/insert", BODY, "batch") == retried
        assert nodes.call(b, "entities/count", BASE)["data"]["count"] == 8
        print(f"M7: recovered {point}, visible before retry = {count}", flush=True)


def async_checkpoint_crash():
    with cluster() as start:
        victim, a = start("channel_next_decision_after_cut")
        nodes.call(
            a,
            "collections/create",
            {**BASE, "dimension": 2, "writeChannels": 4, "autoID": True},
            "c",
        )
        prior = None
        for i in range(140):
            try:
                prior = nodes.call(a, "entities/insert", BODY, f"w{i}")
            except OSError:
                break
        else:
            raise AssertionError("automatic checkpoint failpoint was not reached")
        victim.wait(timeout=20)
        assert victim.returncode != 0 and prior is not None
        _, b = start()
        assert nodes.retry(b, "entities/count", BASE)["data"]["count"] == (i + 1) * 8
        assert nodes.call(b, "entities/insert", BODY, f"w{i - 1}") == prior
        result = nodes.call(b, "entities/insert", BODY, f"w{i}")
        assert result["data"]["insertIds"] == list(range(i * 8 + 1, (i + 1) * 8 + 1))
        assert nodes.call(b, "entities/count", BASE)["data"]["count"] == (i + 1) * 8
        print(
            "M7: published or in-flight checkpoint plus later committed decision recovered",
            flush=True,
        )


def coordinated(folder):
    import cluster_m5_smoke as group
    from pathlib import Path

    group.TMP = Path(folder)
    group.CLUSTER = "m7mtls-" + uuid.uuid4().hex
    group.certificates()
    try:
        leader = group.start("c1", "coordinator", "a")
        w1 = group.start("w1", "node", "a")
        w2 = group.start("w2", "node", "b")
        q1 = group.start("q1", "query-node", "b")
        group.start("q2", "query-node", "a")
        group.wait_for(lambda: group.status(leader)["leader"])
        group.wait_for(
            lambda: group.request(
                w1,
                "collections/create",
                {
                    **BASE,
                    "dimension": 2,
                    "autoID": True,
                    "writeChannels": 4,
                    "shardCount": 4,
                },
                "create",
            )
        )
        state = group.wait_for(lambda: group.active(leader, {"q1", "q2"}))
        for i, w in enumerate([w1, w2]):
            group.wait_for(lambda: group.request(w, "entities/insert", BODY, f"w{i}"))
        assert group.request(q1, "entities/count", BASE)["data"]["count"] == 16
        for address in (w1, w2):
            assert group.status(address)["local"]["stagedWriteBatches"] >= 1
        owner = next(iter(state["state"]["assignments"].values()))["placement"][
            "writer"
        ]
        other = "w2" if owner == "w1" else "w1"
        addr = group.PROCESSES[other][1]
        confirmed = group.wait_for(
            lambda: group.request(addr, "entities/insert", BODY, "confirmed")
        )
        group.stop(owner, kill=True)

        def takeover():
            state = group.active(leader)
            return (
                state
                and next(iter(state["state"]["assignments"].values()))["placement"][
                    "writer"
                ]
                == other
            )

        group.wait_for(takeover)
        assert group.request(addr, "entities/insert", BODY, "confirmed") == confirmed
        assert group.request(q1, "entities/count", BASE)["data"]["count"] == 24
        assert group.request(addr, "entities/insert", BODY, "more")["data"][
            "insertIds"
        ] == list(range(25, 33))
        print(
            "M7: mTLS writer gateways, sharded queries, automatic decision-owner takeover and same-token retry",
            flush=True,
        )
    finally:
        for name in list(group.PROCESSES):
            group.stop(name)
        group.PROCESSES.clear()


if __name__ == "__main__":
    with tempfile.TemporaryDirectory(prefix="vectordb-m7-") as folder:
        nodes.TMP = folder
        try:
            exercise()
            coordinated(folder)
            async_checkpoint_crash()
            with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
                list(
                    pool.map(
                        crash,
                        [
                            "channel_first_prepare",
                            "channels_prepared",
                            "before_channel_decision",
                            "channel_decided",
                            "channel_cut",
                            "checkpoint_published",
                        ],
                    )
                )
        except BaseException:
            for log in nodes.LOGS:
                print(log.name, log.read_text()[-1800:])
            raise
    print(
        "PASS: M7 prepare/decision ordering, atomic reads, retry, takeover and checkpoint reclamation",
        flush=True,
    )
