#!/usr/bin/env python3
"""M5 local multi-process acceptance. Failure domains here are labels, not hosts."""
import json
import argparse
import urllib.request
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import uuid
from cluster_smoke import ROOT, ENV, api, command, port

PROCESSES = {}
LOGS = []
TMP = None
CLUSTER = "m5-" + uuid.uuid4().hex
BINARY = ROOT / "target/debug/vectordb-cluster"


def openssl(*args):
    subprocess.run(["openssl", *args], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def certificates():
    openssl("req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", str(TMP/"ca.key"), "-out", str(TMP/"ca.crt"), "-days", "1", "-subj", "/CN=VectorDB-test-CA")
    for name in ["c1", "c2", "w1", "w2", "q1", "q2", "q3", "rogue"]:
        openssl("req", "-newkey", "rsa:2048", "-nodes", "-keyout", str(TMP/f"{name}.key"), "-out", str(TMP/f"{name}.csr"), "-subj", f"/CN={name}")
        (TMP/f"{name}.ext").write_text(f"basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth,clientAuth\nsubjectAltName=DNS:{name}\n")
        openssl("x509", "-req", "-in", str(TMP/f"{name}.csr"), "-CA", str(TMP/"ca.crt"), "-CAkey", str(TMP/"ca.key"), "-CAcreateserial", "-out", str(TMP/f"{name}.crt"), "-days", "1", "-extfile", str(TMP/f"{name}.ext"))


def start(name, role, zone, addresses=None, crash=None):
    http, rpc = addresses or (port(), port())
    env = {**ENV, "VECTORDB_CLUSTER":CLUSTER, "VECTORDB_COORDINATED":"true", "VECTORDB_NODE_ID":name, "VECTORDB_ZONE":zone, "VECTORDB_RPC_BIND":f"127.0.0.1:{rpc}", "VECTORDB_RPC_ADVERTISE":f"https://127.0.0.1:{rpc}", "VECTORDB_TLS_CA":str(TMP/"ca.crt"), "VECTORDB_TLS_CERT":str(TMP/f"{name}.crt"), "VECTORDB_TLS_KEY":str(TMP/f"{name}.key"), "VECTORDB_MAINTENANCE_MS":"3000"}
    if crash: env["VECTORDB_TEST_CRASH_AT"]=crash
    path=TMP/f"{name}-{len(LOGS)}.log"
    with path.open("w") as stream:
        p=subprocess.Popen([str(BINARY),"--role",role,"--bind",f"127.0.0.1:{http}","--advertise",f"http://127.0.0.1:{http}"],env=env,stdout=stream,stderr=stream)
    PROCESSES[name]=(p,http,rpc,role,zone)
    LOGS.append(path)
    until=time.monotonic()+30
    while time.monotonic()<until:
        assert p.poll() is None,(name,path.read_text())
        try:
            if api(http,"/healthz")[0]==200:return http
        except (OSError,TimeoutError):pass
        time.sleep(.1)
    raise AssertionError((name,"startup timeout",path.read_text()))


def stop(name, kill=False):
    p=PROCESSES[name][0]
    if p.poll() is None:
        p.send_signal(signal.SIGKILL if kill else signal.SIGTERM)
        try:p.wait(timeout=10)
        except subprocess.TimeoutExpired:p.kill();p.wait(timeout=5)


def request(address, op, body=None, key=None):
    status, value=api(address,"/v2/vectordb/"+op,body,key)
    assert status==200,(op,status,value)
    return value


def status(address):
    code,value=api(address,"/cluster/status")
    assert code==200,(code,value)
    return value["data"]


def wait_for(fn, seconds=75):
    until=time.monotonic()+seconds
    error=None
    while time.monotonic()<until:
        try:
            value=fn()
            if value:return value
        except (AssertionError,OSError,TimeoutError) as e:error=e
        time.sleep(.2)
    raise AssertionError(("condition timed out",error))


def active(address, nodes=None):
    s=status(address)
    tasks=list(s["state"]["assignments"].values())
    if not tasks or any(t["phase"]!="Active" or t["error"] for t in tasks):return False
    if nodes and any(not nodes.issubset(set(x for group in t["placement"]["replicas"] for x in group)) for t in tasks):return False
    return s


def acceptance():
    print("M5: mTLS service membership, coordinator election and automatic placement",flush=True)
    c1=start("c1","coordinator","a");c2=start("c2","coordinator","b")
    w1=start("w1","node","a");w2=start("w2","node","b")
    q1=start("q1","query-node","a");q2=start("q2","query-node","b")
    wait_for(lambda:status(c1)["leader"])
    wait_for(lambda:request(w1,"collections/create",{"collectionName":"items","dimension":2,"shardCount":8},"create"))
    state=wait_for(lambda:active(c1,{"q1","q2"}))
    task=next(iter(state["state"]["assignments"].values()));scope=task["scope"]
    request(w2,"entities/insert",{"collectionName":"items","data":[{"id":n,"vector":[n,1]} for n in range(100)]},"batch")
    assert request(q1,"entities/count",{"collectionName":"items"})["data"]["count"]==100
    # No external query is needed for the other replica to catch up.
    def caught_up():
        s=status(c1)
        replicas=s["members"]["q2"]["resources"].get("replicas",[])
        return sum(r["rows"] for r in replicas)==100
    wait_for(caught_up)
    for address in [w1,q1,c1]:
        assert api(address,f"/internal/collections/{scope['collection']}/{scope['generation']}/internal/snapshot",{})[0]==501
    print("M5: certificate identity, membership and protocol rejection",flush=True)
    s=status(c1)
    writer=next(iter(s["state"]["assignments"].values()))["placement"]["writer"]
    def probe(cert="q1", sender="q1", incarnation=None, version=1, cluster=None, ca="ca.crt"):
        args=[str(ROOT/"target/debug/examples/rpc_probe"),f"https://127.0.0.1:{PROCESSES[writer][2]}",writer,str(TMP/ca),str(TMP/f"{cert}.crt") if cert!="-" else "-",str(TMP/f"{cert}.key"),cluster or CLUSTER,sender,incarnation or s["members"]["q1"]["incarnation"],str(version),f"/internal/collections/{scope['collection']}/{scope['generation']}/internal/snapshot"]
        return json.loads(subprocess.check_output(args,text=True))["accepted"]
    assert probe()
    assert not probe(cert="-")
    assert not probe(sender="w1")
    assert not probe(cert="rogue",sender="rogue")
    assert not probe(incarnation="expired-incarnation")
    assert not probe(version=2)
    assert not probe(cluster="foreign-cluster")
    assert not probe(ca="rogue.crt")
    wait_for(lambda:request(w1,"collections/create",{"collectionName":"other","dimension":2,"shardCount":8},"create-other"))
    wait_for(lambda:active(c1,{"q1","q2"}))
    request(w1,"entities/insert",{"collectionName":"other","data":[{"id":1,"vector":[1,1]}]},"batch-other")
    print("M5: warm migration on node join, balanced placement and drain",flush=True)
    q3=start("q3","query-node","c")
    state=wait_for(lambda:active(c1,{"q1","q2","q3"}))
    task=next(iter(state["state"]["assignments"].values()))
    assert all(len(r)==2 for r in task["placement"]["replicas"])
    assert request(q3,"entities/count",{"collectionName":"items"})["data"]["count"]==100
    assert api(c1,"/cluster/nodes/q1/drain",{})[0]==200
    wait_for(lambda:active(c1,{"q2","q3"}) and all("q1" not in r for r in next(iter(status(c1)["state"]["assignments"].values()))["placement"]["replicas"]))
    # Old cache can be retired without retaining an indefinite read pin.
    wait_for(lambda:request(q1,"collections/shard_status",{"collectionName":"items"})["data"]["rows"]==0)
    print("M5: coordinator SIGKILL and automatic writer takeover without client trigger",flush=True)
    leader=status(c1)["leader"];other="c2" if leader=="c1" else "c1";observer=PROCESSES[other][1]
    stop(leader,kill=True)
    wait_for(lambda:status(observer)["leader"]==other)
    task=next(iter(wait_for(lambda:active(observer))["state"]["assignments"].values()))
    writer=task["placement"]["writer"]; survivor="w2" if writer=="w1" else "w1"
    # SIGSTOP preserves the old runtime; lease expiry plus broker fencing must stop it.
    old=PROCESSES[writer][0];old.send_signal(signal.SIGSTOP)
    try:
        def taken_over():
            s=active(observer)
            return s and next(iter(s["state"]["assignments"].values()))["placement"]["writer"]==survivor
        wait_for(taken_over)
        gateway=PROCESSES[survivor][1]
        request(gateway,"entities/upsert",{"collectionName":"items","data":[{"id":999,"vector":[0,1]}]},"after-takeover")
        assert request(q2,"entities/count",{"collectionName":"items"})["data"]["count"]==101
    finally:old.send_signal(signal.SIGCONT)
    wait_for(lambda:api(PROCESSES[writer][1],"/readyz")[0]==503)
    assert api(PROCESSES[writer][1],"/v2/vectordb/entities/insert",{"collectionName":"items","data":[{"id":1000,"vector":[1,0]}]},"old-writer")[0]==503
    print("M5: query failure, degraded service and empty-state replica recovery",flush=True)
    stop("q2",kill=True)
    wait_for(lambda:active(observer) and "q2" not in status(observer)["members"])
    assert request(q3,"entities/count",{"collectionName":"items"})["data"]["count"]==101
    oldq=PROCESSES["q2"];start("q2","query-node","b",(oldq[1],oldq[2]))
    state=wait_for(lambda:active(observer,{"q2","q3"}))
    assert request(oldq[1],"entities/count",{"collectionName":"items"})["data"]["count"]==101
    assert request(q3,"entities/count",{"collectionName":"other"})["data"]["count"]==1
    def maintained():
        t=next(iter(status(observer)["state"]["assignments"].values()))
        return not t["maintenance_pending"] and t["maintenance_completed_ms"]>0
    wait_for(maintained)
    print("M5: real S3 outage leaves maintenance pending, then retries safely",flush=True)
    # Refuse new S3 connections during this outage. A paused process can leave
    # existing TCP connections open until platform-specific transport timeouts.
    command("stop", "-t", "0", "minio")
    try:
        def failed_maintenance():
            tasks = status(observer)["state"]["assignments"]
            return next(((key, task) for key, task in tasks.items()
                         if task["maintenance_pending"] and task["attempts"] > 0 and task["error"]), None)
        key, failed = wait_for(failed_maintenance)
    finally:
        command("start", "minio")

    def retried():
        task = status(observer)["state"]["assignments"][key]
        return (task["maintenance_sequence"] >= failed["maintenance_sequence"]
                and task["maintenance_completed_ms"] > failed["maintenance_completed_ms"]
                and not task["maintenance_pending"] and not task["error"])
    wait_for(retried)
    assert request(q3,"entities/count",{"collectionName":"items"})["data"]["count"]==101
    req=urllib.request.Request(f"http://127.0.0.1:{observer}/metrics",headers={"Authorization":"Bearer "+ENV["VECTORDB_CLUSTER_TOKEN"]})
    with urllib.request.urlopen(req,timeout=10) as response:
        metrics=response.read().decode()
    assert all(name in metrics for name in ["vectordb_pending_assignments", "vectordb_query_wal_lag_offsets", "vectordb_snapshot_wait_ms_total"])
    print("PASS: M5 automatic placement, migration, coordinator failover, writer fencing, query recovery, maintenance and mTLS",flush=True)


def fault_case(boundary):
    global CLUSTER
    for name in list(PROCESSES):stop(name)
    PROCESSES.clear()
    CLUSTER="m5-crash-"+uuid.uuid4().hex
    print("M5: coordinator crash at "+boundary,flush=True)
    c1=start("c1","coordinator","a",crash=boundary)
    wait_for(lambda:status(c1)["leader"]=="c1")
    c2=start("c2","coordinator","b")
    writer=start("w1","node","a")
    query=start("q1","query-node","a")
    wait_for(lambda:request(writer,"collections/create",{"collectionName":"items","dimension":2,"shardCount":4},"create"))
    request(writer,"entities/insert",{"collectionName":"items","data":[{"id":1,"vector":[1,0]}]},"batch")
    wait_for(lambda:PROCESSES["c1"][0].poll() is not None)
    state=wait_for(lambda:active(c2) if status(c2)["leader"]=="c2" else False)
    assert state["leader"]=="c2"
    task=next(iter(state["state"]["assignments"].values()))
    assert task["route_epoch"]==1
    assert request(query,"entities/count",{"collectionName":"items"})["data"]["count"]==1
    # Routing and task completion commit together; restart cannot publish epoch 2.
    time.sleep(1)
    assert next(iter(status(c2)["state"]["assignments"].values()))["route_epoch"]==1
    if boundary=="maintenance_applied":
        wait_for(lambda:next(iter(status(c2)["state"]["assignments"].values()))["maintenance_completed_ms"]>0)
    print("PASS: "+boundary+" resumed without duplicate routing publication",flush=True)


def domain_case():
    global CLUSTER
    for name in list(PROCESSES):stop(name)
    PROCESSES.clear()
    CLUSTER="m5-domain-"+uuid.uuid4().hex
    print("M5: simultaneous loss of a labelled compute failure domain",flush=True)
    c1=start("c1","coordinator","a")
    wait_for(lambda:status(c1)["leader"]=="c1")
    c2=start("c2","coordinator","b")
    w1=start("w1","node","a");w2=start("w2","node","b")
    q1=start("q1","query-node","a");q2=start("q2","query-node","b");start("q3","query-node","c")
    wait_for(lambda:request(w1,"collections/create",{"collectionName":"items","dimension":2,"shardCount":8},"create"))
    wait_for(lambda:active(c1,{"q1","q2","q3"}))
    request(w2,"entities/insert",{"collectionName":"items","data":[{"id":i,"vector":[i,1]} for i in range(100)]},"confirmed")
    started=time.monotonic()
    for name in ["c1","w1","q1"]:stop(name,kill=True)
    def recovered():
        s=active(c2,{"q2","q3"})
        return s and s["leader"]=="c2" and next(iter(s["state"]["assignments"].values()))["placement"]["writer"]=="w2"
    wait_for(recovered)
    assert request(q2,"entities/count",{"collectionName":"items"})["data"]["count"]==100
    request(w2,"entities/upsert",{"collectionName":"items","data":[{"id":100,"vector":[100,1]}]},"after-domain-loss")
    assert request(q2,"entities/count",{"collectionName":"items"})["data"]["count"]==101
    print(f"PASS: compute-domain simulation recovered in {time.monotonic()-started:.2f}s; infrastructure remained available",flush=True)


def main():
    global TMP
    parser=argparse.ArgumentParser()
    parser.add_argument("--faults-only",action="store_true")
    parser.add_argument("--domain-only",action="store_true")
    args=parser.parse_args()
    command("exec","-T","minio","mcli","alias","set","test","http://localhost:9000","development","development-only-password")
    command("exec","-T","minio","mcli","mb","--ignore-existing","test/vectordb")
    with tempfile.TemporaryDirectory(prefix="vectordb-m5-") as path:
        TMP=Path(path);certificates()
        try:
            if not args.faults_only and not args.domain_only:acceptance()
            if not args.domain_only:
                for boundary in ["routing_warmed","routing_published","maintenance_applied"]:fault_case(boundary)
            domain_case()
        except Exception:
            for path in LOGS:print(f"--- {path.name} ---\n{path.read_text()[-12000:]}",flush=True)
            raise
        finally:
            for name,(p,*_) in PROCESSES.items():
                if p.poll() is None:
                    try:p.send_signal(signal.SIGCONT)
                    except ProcessLookupError:pass
                stop(name)

if __name__=="__main__":main()
