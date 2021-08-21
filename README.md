# vectordb

<img src="http://vectordb.io/images/vectordb.jpeg" width="40%">

## What is VectorDB?

As its name, VectorDB gives some operations on vector, such as knn-search (search K Nearest Neighbors). It can be used in AI service, which can help the developers to build their system easily and quickly.

More details at 

official website : [vectordb.io](http://vectordb.io)

wiki : [https://github.com/vectordb-io/vectordb/wiki](https://github.com/vectordb-io/vectordb/wiki)

video : [https://www.bilibili.com/video/BV1HA411w7vb/](https://www.bilibili.com/video/BV1HA411w7vb/)

## Features
| Features | Community Version | Enterprise Version | Comment |
| :---:| :---: | :---: | :--- |
| **data sharding** | o | o | Cut huge data into many small pieces, so that every piece has good performance. |
| **multi programming language SDK** | o | o | C++, Java, Python ... |
| **command console** | o | o | A command console run in linux shell. |
| **web console** | o | o | A web console run in explorer. |
| **plug-in index** | o | o | Several type of indexes can be "plugged" into vectordb. |
| **cluster** |  | o | Deploy vectordb into hundreds of machines, auto failure recovery, auto rebalance. |
| **raft consensus** |  | o | Use raft consensus to replicate data, metadata between several replicas, ensure safety. |
| **transparent index** | o | o | Excellent feature! You need not to build index any more! When a vector is inserted into VectorDB, it can be searched immediately! VectorDB will hold vectors in memory, and merge them into the index at a appropriate time automatically. |
| **data compress** |  | o | Compress the data, save storage spaces. |
| **GPU** |  | o | Build index on GPU, accelerate it! |


## Architecture

#### 1. metadata
<img src="http://vectordb.io/images/metadata2.jpg" width="80%">

#### 2. stand-alone
<img src="http://vectordb.io/images/stand-alone2.jpg" width="70%">

#### 3. cluster
<img src="http://vectordb.io/images/cluster2.jpg" width="80%">

#### 3. class design
<img src="http://vectordb.io/images/class-design2.jpg" width="100%">

## Quick start

#### 1. get executable program

```
wget http://vectordb.io/downloads/packages/vectordb_exe/output.tar.gz
tar zxvf output.tar.gz
```

#### 2. run server

* set log path, if needed. default log path is /tmp
    
```
export GLOG_log_dir=/tmp/vectordb_log
mkdir -p /tmp/vectordb_log
echo $GLOG_log_dir
```

* start server
    
```
./vectordb-server --addr=127.0.0.1:38000 --data_path=/tmp/vectordb
```

#### 3. run client
```
./vectordb-cli --addr=127.0.0.1:38000
(vector-cli) 127.0.0.1:38000>
```

#### 3. create table
```
(vector-cli) 127.0.0.1:38000>  create table {"table_name":"test_table_dim10", "dim":10, "partition_num":8, "replica_num":1}
{
    "code": 0,
    "msg": "create table test_table_dim10 ok"
}
```

#### 4. insert random vectors
```
./vector-inserter 127.0.0.1:38000 test_table_dim10 10 200
```

#### 5. get vector
```
(vector-cli) 127.0.0.1:38000>  get {"table_name":"test_table_dim10", "key":"key0_test"}
{
    "code": 0,
    "msg": "get vector ok",
    "vec_obj": {
        "attach_value1": "inserter_test_attach_value1",
        "attach_value2": "inserter_test_attach_value2",
        "attach_value3": "inserter_test_attach_value3",
        "key": "key0_test",
        "vec": [
            0.350799947977066,
            0.383540153503418,
            0.09649268537759781,
            0.9368243813514709,
            0.4860029518604279,
            0.2966049611568451,
            0.2317648231983185,
            0.4419063627719879,
            0.7600482106208801,
            0.1696054637432098
        ]
    }
}
```

#### 6. build index
```
(vector-cli) 127.0.0.1:38000> build index test_table_dim10;
{
    "code": 0,
    "msg": "build index ok"
}
```

#### 7. get knn (k nearest neighbors)
```
(vector-cli) 127.0.0.1:38000> getknn {"table_name":"test_table_dim10", "key":"key0_test", "limit":20}
{
    "code": 0,
    "msg": "ok",
    "vecdts": [
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0,
            "key": "key0_test"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.4901313781738281,
            "key": "key110_159833662"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.5930646657943726,
            "key": "key163_1640047440"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.6078505516052246,
            "key": "key10_400237917"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.6840280294418335,
            "key": "key13_1660582948"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.6992908716201782,
            "key": "key47_650033517"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7374663352966309,
            "key": "key156_443906634"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7490700483322144,
            "key": "key169_1889879970"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7516026496887207,
            "key": "key142_2006306509"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7525120377540588,
            "key": "key23_1442398070"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7564345002174377,
            "key": "key149_1280540969"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7582194805145264,
            "key": "key156_2046174416"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7703662514686584,
            "key": "key182_1104401994"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7946914434432983,
            "key": "key49_674696936"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.7991812825202942,
            "key": "key180_1241319966"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.8048728108406067,
            "key": "key9_1612326348"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.8286986351013184,
            "key": "key92_40664984"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.8425542712211609,
            "key": "key68_823063584"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.8446422219276428,
            "key": "key146_1651880340"
        },
        {
            "attach_value1": "inserter_test_attach_value1",
            "attach_value2": "inserter_test_attach_value2",
            "attach_value3": "inserter_test_attach_value3",
            "distance": 0.8499314188957214,
            "key": "key45_730060063"
        }
    ]
}
```

## Build from source

#### 1. os environment
```
uname -a
Linux ubuntu 5.4.0-70-generic #78~18.04.1-Ubuntu SMP Sat Mar 20 14:10:07 UTC 2021 x86_64 x86_64 x86_64 GNU/Linux
```

#### 2. install build tools
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

#### 3. build 

```
cd script
sh one_key_build.sh
```

## Develop example

#### 1. cpp

* vectordb/example/cpp/test.cc

```
#include <random>
#include <string>
#include <iostream>
#include <fstream>
#include "status.h"
#include "vdb_client.h"

// use simply call
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
        vectordb_rpc::CreateTableReply reply;
        s = vdb_client.CreateTable(table_name, dim, &reply);
        assert(s.ok());
        std::cout << "create table reply: " << reply.DebugString() << std::endl;
    }

    // put vectors
    std::ofstream outfile("test.data");
    std::string test_key;
    char buf[256];
    for (int i = 0; i < count; ++i) {
        std::string key;
        std::vector<float> vec;
        std::vector<std::string> attach_values;
        vectordb_rpc::PutVecReply reply;

        snprintf(buf, sizeof(buf), "key%d_%d", i, rand());
        key = std::string(buf);
        outfile << key << ", ";

        for (int j = 0; j < dim; ++j) {
            float r = static_cast<float> (rand()) / (static_cast<float>(RAND_MAX));
            vec.push_back(r);
            outfile << r << ", ";
        }
        outfile << std::endl;

        attach_values.push_back("inserter_test_attach_value1");
        attach_values.push_back("inserter_test_attach_value2");
        attach_values.push_back("inserter_test_attach_value3");

        s = vdb_client.PutVec(table_name, key, vec, attach_values, &reply);
        assert(s.ok());
        std::cout << "insert " << key << ", "<< reply.DebugString();

        if (i == 0) {
            test_key = key;
        }
    }
    std::cout << std::endl;

    // build index
    {
        std::cout << "building index ..." << std::endl;
        vectordb_rpc::BuildIndexReply reply;
        s = vdb_client.BuildIndex(table_name, &reply);
        assert(s.ok());
        std::cout << "build index reply: " << reply.DebugString() << std::endl;
    }

    // get
    {
        vectordb_rpc::GetVecRequest request;
        vectordb_rpc::GetVecReply reply;
        request.set_table_name(table_name);
        request.set_key(test_key);
        s = vdb_client.GetVec(request, &reply);
        assert(s.ok());
        std::cout << "get " << test_key << ": "<< reply.DebugString();
    }
    std::cout << std::endl;

    // get knn
    {
        vectordb_rpc::GetKNNReply reply;
        s = vdb_client.GetKNN(table_name, test_key, limit, &reply);
        assert(s.ok());
        std::cout << "getknn " << test_key << ": "<< reply.DebugString();
    }

    return 0;
}
```

* vectordb/example/cpp/test2.cpp

```
#include <random>
#include <string>
#include <iostream>
#include <fstream>
#include "status.h"
#include "vdb_client.h"

// use original grpc call
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
        request.set_table_name(table_name);
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
        std::cout << "building index ..." << std::endl;
        vectordb_rpc::BuildIndexRequest request;
        vectordb_rpc::BuildIndexReply reply;
        request.set_table_name(table_name);
        request.set_index_type(VINDEX_TYPE_ANNOY);
        request.set_distance_type(VINDEX_DISTANCE_TYPE_COSINE);
        request.mutable_annoy_param()->set_tree_num(20);
        s = vdb_client.BuildIndex(request, &reply);
        assert(s.ok());
        std::cout << "build index reply: " << reply.DebugString() << std::endl;
    }

    // get
    {
        vectordb_rpc::GetVecRequest request;
        vectordb_rpc::GetVecReply reply;
        request.set_table_name(table_name);
        request.set_key(test_key);
        s = vdb_client.GetVec(request, &reply);
        assert(s.ok());
        std::cout << "get " << test_key << ": "<< reply.DebugString();
    }

    // get knn
    {
        vectordb_rpc::GetKNNRequest request;
        vectordb_rpc::GetKNNReply reply;
        request.set_table_name(table_name);
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
* vectordb/example/java/test.java

#### 3. python
* vectordb/example/python/test.python


