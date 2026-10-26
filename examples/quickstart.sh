#!/usr/bin/env bash
set -euo pipefail
BASE="${VECTORDB_URL:-http://127.0.0.1:19530}"
auth=(-H 'Content-Type: application/json')
if [[ -n "${VECTORDB_API_KEY:-}" ]]; then auth+=(-H "Authorization: Bearer ${VECTORDB_API_KEY}"); fi
post() { curl --fail-with-body -sS "${auth[@]}" "$BASE/v2/vectordb/$1" -d "$2"; echo; }
post collections/create '{"collectionName":"demo","dimension":3}'
post entities/insert '{"collectionName":"demo","data":[{"id":1,"vector":[1,0,0],"title":"Rust"},{"id":2,"vector":[0,1,0],"title":"Storage"}]}'
post entities/search '{"collectionName":"demo","data":[[1,0,0]],"limit":1,"outputFields":["title"]}'
post entities/query '{"collectionName":"demo","filter":"id >= 1","outputFields":["*"],"limit":10}'
