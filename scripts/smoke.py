#!/usr/bin/env python3
"""Real-process HTTP, SIGKILL recovery, backup and example checks; temporary data only."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get('VECTORDB_BINARY', ROOT / 'target/debug/vectordb')).resolve()
TOKEN = 'smoke-test-only'

def main():
    assert BINARY.is_file(), 'run cargo build --workspace first'
    with tempfile.TemporaryDirectory(prefix='vectordb-smoke-') as temporary:
        tmp = Path(temporary)
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            port = sock.getsockname()[1]
        base = f'http://127.0.0.1:{port}'
        data = tmp / 'db.sqlite'
        process = None
        log = open(tmp / 'server.log', 'wb')
        def stop(hard=False):
            nonlocal process
            if process is not None and process.poll() is None:
                process.send_signal(signal.SIGKILL if hard else signal.SIGTERM)
                process.wait(timeout=20)
            process = None
        def start(path=data, api_key=TOKEN):
            nonlocal process
            environment = dict(os.environ)
            environment.pop('VECTORDB_API_KEY', None)
            if api_key is not None:
                environment['VECTORDB_API_KEY'] = api_key
            process = subprocess.Popen([str(BINARY), 'serve', '--bind', f'127.0.0.1:{port}', '--data', str(path)], env=environment, stdout=log, stderr=log)
            for _ in range(200):
                if process.poll() is not None:
                    raise AssertionError((tmp / 'server.log').read_text())
                try:
                    with urllib.request.urlopen(base + '/healthz', timeout=1) as response:
                        assert json.load(response)['status'] == 'ok'
                        return
                except (urllib.error.URLError, TimeoutError):
                    time.sleep(.05)
            raise AssertionError('server did not start')
        def post(op, body, status=200, token=TOKEN):
            request = urllib.request.Request(base + '/v2/vectordb/' + op, json.dumps(body).encode(), {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + token})
            try:
                response = urllib.request.urlopen(request, timeout=20)
            except urllib.error.HTTPError as error:
                response = error
            with response:
                result = json.load(response)
                assert response.status == status, (op, response.status, result)
                assert (result['code'] == 0) == (status == 200), result
                return result.get('data', result)
        try:
            start()
            post('collections/list', {}, status=401, token='wrong')
            # Run exactly the README quickstart against the authenticated server.
            subprocess.run(['bash', 'examples/quickstart.sh'], cwd=ROOT, env={**os.environ, 'VECTORDB_URL': base, 'VECTORDB_API_KEY': TOKEN}, check=True, stdout=subprocess.DEVNULL)
            assert post('entities/search', {'collectionName':'demo','data':[[1,0,0]],'limit':1})[0]['id'] == 1
            post('collections/create', {'collectionName':'durable','dimension':3,'autoID':True})
            result = post('entities/insert', {'collectionName':'durable','data':[{'vector':[1,0,0]},{'vector':[0,1,0]}]})
            assert result['insertIds'] == [1,2]
            post('entities/insert', {'collectionName':'durable','data':[{'vector':[1,0,0]},{'vector':[1]}]}, status=400)
            # Crash after acknowledgement; do not checkpoint before restarting.
            stop(hard=True)
            start()
            assert post('collections/get_stats', {'collectionName':'durable'})['rowCount'] == 2
            assert post('entities/insert', {'collectionName':'durable','data':[{'vector':[0,0,1]}]})['insertIds'] == [3]
            post('indexes/drop', {'collectionName':'durable','indexName':'vector'})
            post('indexes/create', {'collectionName':'durable','indexParams':[{'fieldName':'vector','indexType':'HNSW'}]})
            expected = post('entities/search', {'collectionName':'durable','data':[[0,0,1]],'limit':1})
            assert expected[0]['id'] == 3
            stop(hard=True)
            start()
            assert post('entities/search', {'collectionName':'durable','data':[[0,0,1]],'limit':1}) == expected
            post('collections/flush', {'collectionName':'durable'})
            post('collections/compact', {'collectionName':'durable'})
            post('entities/query', {'collectionName':'durable','filter':'id =='}, status=400)
            post('collections/describe', {'collectionName':'missing'}, status=404)
            post('users/create', {}, status=501)
            request = urllib.request.Request(base + '/metrics', headers={'Authorization':'Bearer '+TOKEN})
            with urllib.request.urlopen(request) as response:
                assert b'vectordb_requests_total' in response.read()
            # Oversized requests must fail before reaching the engine.
            request = urllib.request.Request(base + '/v2/vectordb/collections/list', b' ' * (16*1024*1024) + b'{}', {'Content-Type':'application/json','Authorization':'Bearer '+TOKEN})
            try:
                urllib.request.urlopen(request, timeout=20)
                raise AssertionError('oversized request unexpectedly accepted')
            except urllib.error.HTTPError as error:
                assert error.code == 413
                assert json.load(error)['code'] == 1100
            stop()
            export = tmp / 'durable.jsonl'
            subprocess.run([str(BINARY),'export','--data',str(data),'--collection','durable','--output',str(export)],check=True,stdout=subprocess.DEVNULL)
            assert len(export.read_text().splitlines()) == 3
            subprocess.run([str(BINARY),'import','--data',str(data),'--collection','durable','--input',str(export),'--upsert','--batch-size','2'],check=True,stdout=subprocess.DEVNULL)
            snapshot = tmp / 'snapshot.sqlite'
            subprocess.run([str(BINARY),'backup','--data',str(data),'--output',str(snapshot)],check=True,stdout=subprocess.DEVNULL)
            start(snapshot)
            assert post('collections/get_stats', {'collectionName':'durable'})['rowCount'] == 3
            assert post('entities/search', {'collectionName':'durable','data':[[0,0,1]],'limit':1}) == expected
            stop()
            start(tmp / 'without-auth.sqlite', api_key=None)
            environment = {**os.environ, 'VECTORDB_URL': base}
            environment.pop('VECTORDB_API_KEY', None)
            subprocess.run(['bash', 'examples/quickstart.sh'], cwd=ROOT, env=environment, check=True, stdout=subprocess.DEVNULL)
            assert post('collections/get_stats', {'collectionName':'demo'})['rowCount'] == 2
            print('PASS: real HTTP, authenticated examples, errors, size limit, SIGKILL recovery, persisted HNSW configuration, JSONL CLI roundtrip, backup restore')
        finally:
            stop()
            log.close()
if __name__ == '__main__':
    main()
