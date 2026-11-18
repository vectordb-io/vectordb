#!/usr/bin/env python3
"""Compare end-to-end single-collection write scaling with constant client concurrency."""

import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import ExitStack, contextmanager
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time
from urllib import error, request
import uuid
import cluster_m3_smoke as nodes
from cluster_smoke import ROOT, TOKEN


def workload(batches, batch, dimension):
    # Generate and encode once, outside the timed client loop.
    return [
        json.dumps(
            {
                "collectionName": "items",
                "data": [
                    {
                        "id": i,
                        "vector": [
                            ((i * 13 + j * 17) % 256) / 256 for j in range(dimension)
                        ],
                    }
                    for i in range(b * batch, (b + 1) * batch)
                ],
            },
            separators=(",", ":"),
        ).encode()
        for b in range(batches)
    ]


@contextmanager
def compute_node(cluster, image, cpus, ordinal):
    if not image:
        process, port = nodes.start(cluster)
        try:
            yield port
        finally:
            nodes.stop(process)
        return
    name = f"{cluster}-n{ordinal}"
    port = nodes.port()
    args = [
        "docker",
        "run",
        "--detach",
        "--name",
        name,
        "--network",
        "vectordb-cluster-test_default",
        "--cpus",
        str(cpus),
        "--memory",
        "2g",
        "--publish",
        f"127.0.0.1:{port}:19531",
    ]
    env = {
        "VECTORDB_CLUSTER": cluster,
        "VECTORDB_ETCD": "http://etcd:2379",
        "VECTORDB_KAFKA": "kafka:9094",
        "VECTORDB_S3_ENDPOINT": "http://minio:9000",
        "VECTORDB_S3_ACCESS_KEY": "development",
        "VECTORDB_S3_SECRET_KEY": "development-only-password",
        "VECTORDB_CLUSTER_TOKEN": TOKEN,
    }
    for k, v in env.items():
        args.extend(["--env", f"{k}={v}"])
    subprocess.run(
        args
        + [
            image,
            "--role",
            "node",
            "--bind",
            "0.0.0.0:19531",
            "--advertise",
            f"http://{name}:19531",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    try:
        ready = time.monotonic() + 45
        while time.monotonic() < ready:
            try:
                if nodes.api(port, "/readyz")[0] == 200:
                    break
            except OSError:
                pass
            time.sleep(0.1)
        else:
            raise RuntimeError("container did not become ready")
        yield port
    finally:
        subprocess.run(
            ["docker", "rm", "-f", name], check=True, stdout=subprocess.DEVNULL
        )


def run(processes, channels, inputs, batch, dimension, concurrency, image=None):
    cluster = "m7bench-" + uuid.uuid4().hex
    with ExitStack() as stack:
        # In the quota-controlled profile, keep the same decision and checkpoint
        # resources across all runs, and vary only the number of ingestion CPUs.
        owner = stack.enter_context(compute_node(cluster, image, 3, processes))
        base = {"collectionName": "items"}
        definition = {
            **base,
            "dimension": dimension,
            "indexParams": [
                {"fieldName": "vector", "metricType": "L2", "indexType": "FLAT"}
            ],
        }
        if channels:
            definition["writeChannels"] = channels
        nodes.retry(owner, "collections/create", definition, "create")
        ports = [] if image else [owner]
        for i in range(processes - len(ports)):
            ports.append(stack.enter_context(compute_node(cluster, image, 1, i)))
        for i in range(4):
            port = ports[i % len(ports)]
            nodes.call(
                port,
                "entities/upsert",
                {**base, "data": [{"id": -1 - i, "vector": [0] * dimension}]},
                f"warm{i}",
            )
        nodes.call(owner, "collections/flush", base)
        issued = int(time.time() * 1000)

        def write(i):
            addr = ports[i % len(ports)]
            req = request.Request(
                f"http://127.0.0.1:{addr}/v2/vectordb/entities/insert",
                inputs[i],
                {
                    "Content-Type": "application/json",
                    "Authorization": "Bearer " + TOKEN,
                    "Idempotency-Key": f"v2-{issued}-batch{i}",
                },
            )
            before = time.monotonic()
            for n in range(10):
                try:
                    with request.urlopen(req, timeout=60) as resp:
                        value = json.load(resp)
                    assert value["data"]["insertIds"] == list(
                        range(i * batch, (i + 1) * batch)
                    )
                    return (time.monotonic() - before) * 1000, n
                except error.HTTPError as e:
                    if e.code != 503:
                        raise RuntimeError(e.read()) from e
                    time.sleep(0.05)
            raise RuntimeError("same-key retry budget exhausted")

        start = time.monotonic()
        with ThreadPoolExecutor(max_workers=concurrency) as pool:
            samples = list(pool.map(write, range(len(inputs))))
        ack = time.monotonic() - start
        nodes.call(owner, "collections/flush", base)
        flushed = time.monotonic() - start
        assert (
            nodes.call(owner, "entities/count", base)["data"]["count"]
            == len(inputs) * batch + 4
        )
        results = sorted(s[0] for s in samples)
        total = len(inputs) * batch
        result = dict(
            writer_gateways=processes,
            write_channels=channels,
            rows=total,
            ack_seconds=ack,
            flush_seconds=flushed,
            ack_rows_per_s=total / ack,
            flushed_rows_per_s=total / flushed,
            p50_batch_ms=statistics.median(results),
            p99_batch_ms=results[(99 * len(results) + 99) // 100 - 1],
            same_key_retries=sum(s[1] for s in samples),
        )
        print(json.dumps(result), flush=True)
        return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary", type=Path, default=ROOT / "target/release/vectordb-cluster"
    )
    parser.add_argument(
        "--writer-image",
        help="Linux image for fixed 1-CPU ingestion nodes and a dedicated 3-CPU decision owner",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--batches", type=int, default=256)
    parser.add_argument("--batch", type=int, default=128)
    parser.add_argument("--dimension", type=int, default=128)
    parser.add_argument("--concurrency", type=int, default=8)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--channels", type=int, default=4)
    args = parser.parse_args()
    nodes.BINARY = args.binary.resolve()
    inputs = workload(args.batches, args.batch, args.dimension)
    report = dict(
        platform=platform.platform(),
        cpu=platform.processor(),
        logical_cpus=os.cpu_count(),
        base_commit=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        input_sha256=hashlib.sha256(b"".join(inputs)).hexdigest(),
        batch=args.batch,
        dimension=args.dimension,
        concurrency=args.concurrency,
        distribution="uniform grid, i*13+j*17 modulo 256",
        rounds=args.rounds,
        topology="One host, shared single broker, etcd and S3. Fixed total offered concurrency, independent writer gateway processes, one collection decision owner.",
        results=[],
    )
    if args.writer_image:
        report.update(
            image_id=subprocess.check_output(
                [
                    "docker",
                    "image",
                    "inspect",
                    "--format",
                    "{{.Id}}",
                    args.writer_image,
                ],
                text=True,
            ).strip(),
            docker_vm=subprocess.check_output(
                ["docker", "info", "--format", "{{.NCPU}} CPUs, {{.MemTotal}} bytes"],
                text=True,
            ).strip(),
            resources="Dedicated decision owner 3 CPUs/2 GiB, each ingestion gateway 1 CPU/2 GiB, enforced by Docker cgroups.",
        )
    else:
        report["binary_sha256"] = hashlib.sha256(nodes.BINARY.read_bytes()).hexdigest()
    if platform.system() == "Darwin":
        report.update(
            cpu=subprocess.check_output(
                ["sysctl", "-n", "machdep.cpu.brand_string"], text=True
            ).strip(),
            memory_bytes=int(subprocess.check_output(["sysctl", "-n", "hw.memsize"])),
        )
    with tempfile.TemporaryDirectory(prefix="vectordb-m7-benchmark-") as folder:
        nodes.TMP = folder
        configs = [
            (1, None),
            (1, args.channels),
            (2, args.channels),
            (4, args.channels),
        ]
        for r in range(args.rounds):
            for writers, channels in (
                configs[r % len(configs) :] + configs[: r % len(configs)]
            ):
                report["results"].append(
                    dict(
                        round=r + 1,
                        **run(
                            writers,
                            channels,
                            inputs,
                            args.batch,
                            args.dimension,
                            args.concurrency,
                            args.writer_image,
                        ),
                    )
                )
                args.output.write_text(json.dumps(report, indent=2) + "\n")
