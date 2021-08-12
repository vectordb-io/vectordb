#include <random>
#include <string>
#include <iostream>
#include <fstream>
#include "status.h"
#include "vdb_client.h"

std::string exe_name;

void Usage() {
    std::cout << std::endl;
    std::cout << "Usage: " << std::endl;
    std::cout << exe_name << " address table_name dim count" << std::endl;
    std::cout << exe_name << " 127.0.0.1:38000 test_table_dim10 10 200" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char **argv) {
    vectordb::Status s;
    FLAGS_alsologtostderr = false;
    exe_name = std::string(argv[0]);
    srand(static_cast<unsigned>(time(nullptr)));

    if (argc < 5) {
        Usage();
        exit(-1);
    }

    int dim, count;
    std::string address, table_name, out_put_file;

    address = argv[1];
    table_name = argv[2];
    sscanf(argv[3], "%d", &dim);
    sscanf(argv[4], "%d", &count);
    out_put_file = table_name + "." + std::string(argv[3]) + "." + std::string(argv[4]) + ".txt";

    vectordb::VdbClient vdb_client(address);
    s = vdb_client.Connect();
    assert(s.ok());

    std::ofstream outfile(out_put_file);

    char buf[256];
    for (int i = 0; i < count; ++i) {
        std::string key;
        vectordb_rpc::PutVecRequest request;
        request.set_table_name(table_name);
        request.mutable_vec_obj()->set_attach_value1("inserter_test_attach_value1");
        request.mutable_vec_obj()->set_attach_value2("inserter_test_attach_value2");
        request.mutable_vec_obj()->set_attach_value3("inserter_test_attach_value3");

        if (i == 0) {
            snprintf(buf, sizeof(buf), "key%d_test", i);
        } else {
            snprintf(buf, sizeof(buf), "key%d_%d", i, rand());
        }

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
        std::cout << "insert " << key << ", "<< reply.DebugString();
    }
    std::cout << std::endl <<"output file:" << out_put_file << std::endl << std::endl;

    return 0;
}
