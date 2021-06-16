# vectordb

<img src="images/vectordb.jpeg" width="40%">

## What is VectorDB?

Vectordb is an open-source database used in AI service. It supports operations on KV, vector and graph, which can help the developers to build their AI service easily and quickly.
<br>
<br>
More details at [vectordb.io](http://vectordb.io)

## Quick start

1. start server

```
./vectordb-server --addr=127.0.0.1:38000 --data_path=/tmp/vectordb
```

2. start client

```
./vectordb-cli --addr=127.0.0.1:38000
(vector-cli) 127.0.0.1:38000>
```

2. show help

```
(vector-cli) 127.0.0.1:38000> help
-------------------------------------------------------------------------
console command example:

help
info
ping
exit
quit
version
show tables
desc table_name
desc partition_name
desc replica_name

create table {"table_name":"vector_table", "engine_type":"vector", "dim":4, "partition_num":10, "replica_num":1}
put {"table_name":"vector_table", "key":"kkk", "vector":[1.13, 2.25, 3.73, 4.99], "attach_value1":"attach_value1", "attach_value2":"attach_value2", "attach_value3":"attach_value3"}
build index {"table_name":"vector_table", "index_type":"annoy"}
build index {"table_name":"vector_table", "index_type":"knn_graph", "k":100}
get {"table_name":"vector_table", "key":"kkk"}
getknn {"table_name":"vector_table", "key":"kkk", "limit":20, "index_name":"my_index"}
distance key {"table_name":"vector_table", "key1":"xxx", "key2":"ooo"}
distance vector {"vector1":[1.13, 2.25, 3.73, 4.99], "vector2":[3.93, 9.27, 4.63, 2.91]}

keys {"table_name":"kv_table"}
create table {"table_name":"kv_table", "engine_type":"kv", "partition_num":1, "replica_num":1}
put {"table_name":"kv_table", "key":"kkk", "value":"vvv"}
get {"table_name":"kv_table", "key":"kkk"}
del {"table_name":"kv_table", "key":"kkk"}

create table {"table_name":"graph_table", "engine_type":"graph", "partition_num":1, "replica_num":1}
-------------------------------------------------------------------------
```

3. create table

```
(vector-cli) 127.0.0.1:38000> create table {"table_name":"test_vector_table", "engine_type":"vector", "dim":10, "partition_num":10, "replica_num":3}
{
    "code": 0,
    "msg": "create table test_vector_table ok"
}
```


4. put vectors

5. get vector

6. build index

7. get knn (k nearest neighbors)

## Build from source 
1. build dependencies
2. cd vectordb/src && make

## Architecture
