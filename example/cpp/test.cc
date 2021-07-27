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
