# vectordb

<img src="http://vectordb.io/images/vectordb.jpeg" width="40%">

## What is VectorDB?

Vectordb is an open-source database used in AI service. It supports operations on KV, vector and graph, which can help the developers to build their AI service easily and quickly.
<br>
<br>
More details at [vectordb.io](http://vectordb.io)


## Features
#### 1. multiple data structure
* kv / document
* vector
* graph

#### 2. distributed storage
* a huge store
* use raft to ensure safty
* auto recovery, auto rebalance
* high throughput, low latency

## Architecture

#### 1. metadata
<img src="http://vectordb.io/images/metadata.jpg" width="80%">

#### 2. stand-alone
<img src="http://vectordb.io/images/stand-alone.jpg" width="70%">

#### 3. cluster
<img src="http://vectordb.io/images/cluster.jpg" width="80%">

## Quick start

#### 0. get executable program
```
wget http://vectordb.io/downloads/packages/vectordb_exe/vectordb.tar.gz
tar zxvf vectordb.tar.gz
```

#### 1. run server
```
./vectordb-server --addr=127.0.0.1:38000 --data_path=/tmp/vectordb
```

#### 2. run client
```
./vectordb-cli --addr=127.0.0.1:38000
(vector-cli) 127.0.0.1:38000>
```

#### 3. create table
```
(vector-cli) 127.0.0.1:38000> create table {"table_name":"test_vector_table", "engine_type":"vector", "dim":10, "partition_num":10, "replica_num":3}
{
    "code": 0,
    "msg": "create table test_vector_table ok"
}
```

#### 4. generate random vectors
```
./vector-inserter 127.0.0.1:38000 test_vector_table 10 100
```

#### 5. get vector
```
(vector-cli) 127.0.0.1:38000> get {"table_name":"test_vector_table", "key":"key72_261551668"}
{
    "code": 0,
    "msg": "get vector ok",
    "vec_obj": {
        "attach_value1": "inserter_test_attach_value1",
        "attach_value2": "inserter_test_attach_value2",
        "attach_value3": "inserter_test_attach_value3",
        "key": "key72_261551668",
        "vec": [
            0.6292882057043203,
            0.5495009015079126,
            0.9620135063128609,
            0.1219228967660679,
            0.6931321652108487,
            0.8820888348306012,
            0.2154071872194331,
            0.8231134846914157,
            0.4136770024959356,
            0.4983163487623988
        ]
    }
}
```

#### 6. build index
```
(vector-cli) 127.0.0.1:38000> build index {"table_name":"test_vector_table", "index_type":"knn_graph", "k":20}
{
    "code": 0,
    "msg": "ok"
}
```

#### 7. view metadata
```
(vector-cli) 127.0.0.1:38000> show tables
{
    "tables": [
        "test_vector_table"
    ]
}
```

```
(vector-cli) 127.0.0.1:38000> desc test_vector_table
{
    "code": 0,
    "msg": "desc test_vector_table ok",
    "table": {
        "dim": 10,
        "engine_type": "vector",
        "indices": [
            {
                "index_name": "knn_graph1623826143",
                "index_type": "knn_graph"
            }
        ],
        "name": "test_vector_table",
        "partition_num": 10,
        "partitions": [
            "test_vector_table#partition_0",
            "test_vector_table#partition_1",
            "test_vector_table#partition_2",
            "test_vector_table#partition_3",
            "test_vector_table#partition_4",
            "test_vector_table#partition_5",
            "test_vector_table#partition_6",
            "test_vector_table#partition_7",
            "test_vector_table#partition_8",
            "test_vector_table#partition_9"
        ],
        "path": "/tmp/vectordb/data/test_vector_table",
        "replica_num": 3
    }
}
```
```
(vector-cli) 127.0.0.1:38000> desc test_vector_table#partition_0
{
    "code": 0,
    "msg": "desc test_vector_table#partition_0 ok",
    "partition": {
        "id": 0,
        "name": "test_vector_table#partition_0",
        "path": "/tmp/vectordb/data/test_vector_table/0",
        "replica_num": 3,
        "replicas": [
            "test_vector_table#partition_0#replica_0",
            "test_vector_table#partition_0#replica_1",
            "test_vector_table#partition_0#replica_2"
        ],
        "table_name": "test_vector_table"
    }
}
```
```
(vector-cli) 127.0.0.1:38000> desc test_vector_table#partition_0#replica_0
{
    "code": 0,
    "msg": "desc test_vector_table#partition_0#replica_0 ok",
    "replica": {
        "address": "127.0.0.1:38000",
        "id": 0,
        "name": "test_vector_table#partition_0#replica_0",
        "partition_name": "test_vector_table#partition_0",
        "path": "/tmp/vectordb/data/test_vector_table/0/0",
        "table_name": "test_vector_table"
    }
}
```

#### 8. get knn (k nearest neighbors)
```
(vector-cli) 127.0.0.1:38000> getknn {"table_name":"test_vector_table", "key":"key82_640136302", "limit":5, "index_name":"knn_graph1623826143"}
{
    "code": 0,
    "msg": "ok",
    "vecdts": [
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0,
            "key": "key82_640136302"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.3952870669883494,
            "key": "key39_2094485213"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.4180229187303537,
            "key": "key95_1693950771"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.4221731709119398,
            "key": "key88_344836378"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.4231269304129058,
            "key": "key62_1717661487"
        }
    ]
}
```

## Build from source
#### 0. os environment
```
uname -a
Linux ubuntu 5.4.0-70-generic #78~18.04.1-Ubuntu SMP Sat Mar 20 14:10:07 UTC 2021 x86_64 x86_64 x86_64 GNU/Linux
```

#### 1. install build tools
```
sudo apt install autoconf automake libtool -y
sudo apt install pkg-config -y

sudo apt install libssl-dev -y
wget https://cmake.org/files/v3.18/cmake-3.18.4.tar.gz
tar zxvf cmake-3.18.4.tar.gz
cd cmake-3.18.4/
./bootstrap
make -j 4
sudo make install

cmake -version
cmake version 3.18.4
```

#### 2. build source code

```
cd script
sh one_key_build.sh
```

## develop example

#### 1. cpp
* vectordb/example/cpp/test.cpp
```
#include <random>
#include <string>
#include <iostream>
#include <fstream>
#include "status.h"
#include "vdb_client.h"


int main(int argc, char **argv) {
    vectordb::Status s;
    srand(static_cast<unsigned>(time(nullptr)));

    int dim, count, limit;
    std::string address, table_name;

    address = "127.0.0.1:38000";
    table_name = "test_table_dim10";
    count = 1000;
    dim = 10;
    limit = 5;

    vectordb::VdbClient vdb_client(address);
    s = vdb_client.Connect();
    assert(s.ok());

    // create table
    {
        vectordb_rpc::CreateTableRequest request;
        vectordb_rpc::CreateTableReply reply;
        request.set_table_name(table_name);
        request.set_partition_num(1);
        request.set_replica_num(1);
        request.set_engine_type("vector");
        request.set_dim(dim);

        s = vdb_client.CreateTable(request, &reply);
        assert(s.ok());
        std::cout << "create table reply: " << reply.DebugString() << std::endl;
    }

    // put vectors
    std::ofstream outfile("test.data");
    std::string test_key;
    char buf[256];
    for (int i = 0; i < count; ++i) {
        std::string key;
        vectordb_rpc::PutVecRequest request;
        request.set_table(table_name);
        request.mutable_vec_obj()->set_attach_value1("inserter_test_attach_value1");
        request.mutable_vec_obj()->set_attach_value2("inserter_test_attach_value2");
        request.mutable_vec_obj()->set_attach_value3("inserter_test_attach_value3");

        snprintf(buf, sizeof(buf), "key%d_%d", i, rand());
        key = std::string(buf);
        request.mutable_vec_obj()->set_key(key);

        outfile << key << ", ";
        for (int j = 0; j < dim; ++j) {
            double r = static_cast<double> (rand()) / (static_cast<double>(RAND_MAX));
            outfile << r << ", ";
            request.mutable_vec_obj()->mutable_vec()->add_data(r);
        }
        outfile << std::endl;

        vectordb_rpc::PutVecReply reply;
        s = vdb_client.PutVec(request, &reply);
        assert(s.ok());
        std::cout << "insert " << key << ", "<< reply.DebugString();

        if (i == 0) {
            test_key = key;
        }
    }
    std::cout << std::endl;

    // build index
    {
        vectordb_rpc::BuildIndexRequest request;
        vectordb_rpc::BuildIndexReply reply;
        request.set_table(table_name);
        request.set_index_type("annoy");
        s = vdb_client.BuildIndex(request, &reply);
        assert(s.ok());
        std::cout << "build index reply: " << reply.DebugString() << std::endl;
    }

    // get
    {
        vectordb_rpc::GetVecRequest request;
        vectordb_rpc::GetVecReply reply;
        request.set_table(table_name);
        request.set_key(test_key);
        s = vdb_client.GetVec(request, &reply);
        assert(s.ok());
        std::cout << "get " << test_key << ": "<< reply.DebugString();
    }

    // get knn
    {
        vectordb_rpc::GetKNNRequest request;
        vectordb_rpc::GetKNNReply reply;
        request.set_table(table_name);
        request.set_key(test_key);
        request.set_index_name("default");
        request.set_limit(limit);
        std::cout << "request: " << request.DebugString();
        s = vdb_client.GetKNN(request, &reply);
        assert(s.ok());
        std::cout << "getknn " << test_key << ": "<< reply.DebugString();
    }

    return 0;
}
```

#### 2. java

#### 3. python


