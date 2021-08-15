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
