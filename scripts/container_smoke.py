#!/usr/bin/env python3
"""Run an existing local image with an isolated anonymous volume; clean up afterward."""
import json
import os
import subprocess
import time
import urllib.error
import urllib.request
import uuid

image = os.environ.get('VECTORDB_DOCKER_IMAGE', 'vectordb:local')
name = 'vectordb-smoke-' + uuid.uuid4().hex[:10]
def docker(*args):
    return subprocess.check_output(['docker', *args], text=True).strip()
def ready():
    for _ in range(100):
        state = json.loads(docker('inspect','--format','{{json .State}}',name))
        if not state['Running']:
            logs = docker('logs','--tail','50',name)
            raise AssertionError(f"container exited ({state['ExitCode']}): {logs}")
        ports = json.loads(docker('inspect','--format','{{json .NetworkSettings.Ports}}',name))
        bindings = (ports or {}).get('19530/tcp')
        if not bindings:
            time.sleep(.1)
            continue
        base = 'http://127.0.0.1:' + bindings[0]['HostPort']
        try:
            with urllib.request.urlopen(base + '/healthz',timeout=1) as response:
                assert json.load(response)['status'] == 'ok'
                return base
        except (urllib.error.URLError,TimeoutError):
            time.sleep(.1)
    raise AssertionError('container did not become healthy')
def post(base,op,body):
    request=urllib.request.Request(base+'/v2/vectordb/'+op,json.dumps(body).encode(),{'Content-Type':'application/json'})
    with urllib.request.urlopen(request,timeout=10) as response:
        result=json.load(response)
        assert result['code']==0,result
        return result['data']
try:
    docker('run','--detach','--name',name,'-p','127.0.0.1::19530',image)
    assert docker('inspect','--format','{{.Config.User}}',name)=='10001:10001'
    base=ready()
    post(base,'collections/create',{'collectionName':'container_check','dimension':2})
    post(base,'entities/insert',{'collectionName':'container_check','data':[{'id':1,'vector':[1,0]}]})
    docker('kill','--signal=KILL',name)
    docker('start',name)
    base=ready()
    result=post(base,'entities/search',{'collectionName':'container_check','data':[[1,0]],'limit':1})
    assert result[0]['id']==1 and result[0]['distance']==1.0,result
    print('PASS: non-root container, HTTP create/insert/search, anonymous-volume persistence across SIGKILL/restart')
except Exception:
    subprocess.run(['docker','logs','--tail','50',name],check=False)
    raise
finally:
    subprocess.run(['docker','rm','--force','--volumes',name],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
