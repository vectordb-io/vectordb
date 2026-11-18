#!/usr/bin/env python3
"""Exercise the chart on an isolated local kind cluster and the M6 Compose backends."""

import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import time
import uuid

import cluster_m5_smoke as tls
from cluster_smoke import ROOT, ENV


def run(cmd, **kwargs):
    return subprocess.run(
        cmd, check=True, text=True, capture_output=True, timeout=180, **kwargs
    ).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--kind",
        required=True,
        help="dedicated existing kind cluster with compute image already loaded",
    )
    parser.add_argument("--image", default="vectordb-cluster:m7")
    args = parser.parse_args()
    context = "kind-" + args.kind
    ns = "vdb-m6-" + uuid.uuid4().hex[:8]
    kube = ["kubectl", "--context", context, "--namespace", ns]
    chart = ROOT / "deployments/helm/vectordb"
    attached = []
    run(kube + ["create", "namespace", ns])
    try:
        nodes = json.loads(run(kube + ["get", "nodes", "-o", "json"]))["items"]
        assert len(nodes) == 1, (
            "this local fixture requires a dedicated single-node kind cluster"
        )
        run(
            kube
            + [
                "label",
                "node",
                nodes[0]["metadata"]["name"],
                "topology.kubernetes.io/zone=local",
                "--overwrite",
            ]
        )
        for service, ports in [("etcd", 2379), ("kafka", 9094), ("minio", 9000)]:
            container = f"vectordb-cluster-test-{service}-1"
            data = json.loads(run(["docker", "inspect", container]))[0][
                "NetworkSettings"
            ]["Networks"]
            if "kind" not in data:
                run(["docker", "network", "connect", "kind", container])
                attached.append(container)
            ip = json.loads(run(["docker", "inspect", container]))[0][
                "NetworkSettings"
            ]["Networks"]["kind"]["IPAddress"]
            run(kube + ["create", "service", "clusterip", service, "--tcp", str(ports)])
            # The selector created by kubectl targets no pods. Supply the backend address explicitly.
            run(
                kube
                + [
                    "patch",
                    "service",
                    service,
                    "--type=json",
                    "-p",
                    '[{"op":"remove","path":"/spec/selector"}]',
                ]
            )
            portname = json.loads(
                run(kube + ["get", "service", service, "-o", "json"])
            )["spec"]["ports"][0]["name"]
            resource = {
                "apiVersion": "discovery.k8s.io/v1",
                "kind": "EndpointSlice",
                "metadata": {
                    "name": service,
                    "labels": {"kubernetes.io/service-name": service},
                },
                "addressType": "IPv4",
                "ports": [{"name": portname, "protocol": "TCP", "port": ports}],
                "endpoints": [{"addresses": [ip], "conditions": {"ready": True}}],
            }
            run(kube + ["apply", "-f", "-"], input=json.dumps(resource))
        with tempfile.TemporaryDirectory(prefix="vectordb-helm-") as tmp:
            tls.TMP = Path(tmp)
            tls.certificates()
            members = []
            for id, role in [
                ("c1", "coordinator"),
                ("c2", "coordinator"),
                ("w1", "node"),
                ("w2", "node"),
                ("q1", "query-node"),
                ("q2", "query-node"),
            ]:
                run(
                    kube
                    + [
                        "create",
                        "secret",
                        "generic",
                        id,
                        "--from-file=ca.crt=" + tmp + "/ca.crt",
                        "--from-file=tls.crt=" + tmp + "/" + id + ".crt",
                        "--from-file=tls.key=" + tmp + "/" + id + ".key",
                    ]
                )
                members.append(dict(id=id, role=role, zone="local", tlsSecret=id))
            credentials = {
                k: ENV[k]
                for k in [
                    "VECTORDB_CLUSTER_TOKEN",
                    "VECTORDB_S3_ACCESS_KEY",
                    "VECTORDB_S3_SECRET_KEY",
                ]
            }
            credentials.update(
                VECTORDB_READ_TOKEN="helm-read-fixture-token",
                VECTORDB_WRITE_TOKEN="helm-write-fixture-token",
            )
            run(
                kube + ["apply", "-f", "-"],
                input=json.dumps(
                    dict(
                        apiVersion="v1",
                        kind="Secret",
                        metadata=dict(name="vectordb-credentials"),
                        stringData=credentials,
                    )
                ),
            )
            values = Path(tmp) / "values.json"
            values.write_text(
                json.dumps(
                    dict(
                        cluster=ns,
                        image=args.image,
                        members=members,
                        etcd="http://etcd:2379",
                        kafka="kafka:9094",
                        walReplicas=1,
                        s3Endpoint="http://minio:9000",
                        resources={
                            "requests": {"cpu": "50m", "memory": "64Mi"},
                            "limits": {"cpu": "2", "memory": "512Mi"},
                        },
                    )
                )
            )
            cmd = [
                "helm",
                "--kube-context",
                context,
                "-n",
                ns,
                "upgrade",
                "--install",
                "m6",
                str(chart),
                "-f",
                str(values),
                "--wait",
                "--timeout",
                "150s",
            ]
            run(cmd)
            run(cmd)
            run(
                kube
                + [
                    "wait",
                    "--for=condition=Ready",
                    "pod",
                    "-l",
                    "app.kubernetes.io/instance=m6",
                    "--timeout=120s",
                ]
            )

            def retry(op, body, key=None):
                last = None
                until = time.monotonic() + 90
                idempotency = f"v2-{int(time.time() * 1000)}-{key or 'read'}"
                while time.monotonic() < until:
                    try:
                        command = 'curl -sS --max-time 35 -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN" -H "Content-Type: application/json" -H "Idempotency-Key: $2" --data-binary @- "http://m6-gateway:19531/v2/vectordb/$1"'
                        value = json.loads(
                            run(
                                kube
                                + [
                                    "exec",
                                    "-i",
                                    "m6-w1-0",
                                    "--",
                                    "sh",
                                    "-ec",
                                    command,
                                    "sh",
                                    op,
                                    idempotency,
                                ],
                                input=json.dumps(body),
                            )
                        )
                        last = value
                        if value["code"] == 0:
                            return value["data"]
                        assert value["code"] == 503, value
                    except subprocess.CalledProcessError as error:
                        last = error.stderr
                    time.sleep(0.4)
                raise AssertionError((op, last))

            base = {"collectionName": "fixture"}
            retry("collections/create", {**base, "dimension": 2}, "create")
            retry(
                "entities/insert",
                {**base, "data": [{"id": 1, "vector": [0, 1]}]},
                "insert",
            )
            assert retry("entities/count", base)["count"] == 1
            print(
                "PASS: Helm install and upgrade, six ready pods, mTLS, routed write and Strong read"
            )
    except BaseException:
        for pod in json.loads(run(kube + ["get", "pods", "-o", "json"]))["items"]:
            print(
                pod["metadata"]["name"],
                run(kube + ["logs", pod["metadata"]["name"], "--tail=12"]),
            )
        print(run(kube + ["get", "pods", "-o", "wide"]))
        print(run(kube + ["get", "events", "--sort-by=.lastTimestamp"])[-3500:])
        raise
    finally:
        run(kube + ["delete", "namespace", ns, "--wait=false"])
        for container in attached:
            run(["docker", "network", "disconnect", "kind", container])


if __name__ == "__main__":
    main()
