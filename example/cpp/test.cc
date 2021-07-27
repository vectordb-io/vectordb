#include <random>
#include <string>
#include <iostream>
#include <fstream>
#include "status.h"
#include "vdb_client.h"


int main(int argc, char **argv) {
    vectordb::Status s;
    srand(static_cast<unsigned>(time(nullptr)));

    int dim, count;
    std::string address, table_name;

    address = "127.0.0.1";
    table_name = "test_table_dim10";
    count = 10000;

    vectordb::VdbClient vdb_client(address);
    s = vdb_client.Connect();
    assert(s.ok());

    // create table
    {
        vectordb_rpc::CreateTableRequest request;
        vectordb_rpc::CreateTableReply reply;
        s = vdb_client.CreateTable(request, &reply);
        assert(s.ok());
    }

    // put vectors
    std::ofstream outfile("test.data");
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
    }

    // build index
    {
        vectordb_rpc::BuildIndexRequest request;
        vectordb_rpc::BuildIndexReply reply;
        s = vdb_client.BuildIndex(request, &reply);
        assert(s.ok());
    }

    // get
    {
        vectordb_rpc::GetVecRequest request;
        vectordb_rpc::GetVecReply reply;
        s = vdb_client.GetVec(request, &reply);
        assert(s.ok());
    }

    // get knn
    {
        vectordb_rpc::GetKNNRequest request;
        vectordb_rpc::GetKNNReply reply;
        s = vdb_client.GetKNN(request, &reply);
        assert(s.ok());
    }

    return 0;
}
