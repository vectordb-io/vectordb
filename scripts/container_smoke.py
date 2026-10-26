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
    address = docker('port',name,'19530/tcp').splitlines()[0]
    base = 'http://' + address
    for _ in range(100):
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
    print('PASS: non-root container, HTTP CRUD/search, anonymous-volume persistence across SIGKILL/restart')
finally:
    subprocess.run(['docker','rm','--force','--volumes',name],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=False)
