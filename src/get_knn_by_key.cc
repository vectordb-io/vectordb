#include <random>
#include <string>
#include <iostream>
#include <glog/logging.h>
#include "status.h"
#include "vdb_client.h"

std::string exe_name;

void Usage() {
    std::cout << std::endl;
    std::cout << "Usage: " << std::endl;
    std::cout << exe_name << " address table_name index_name limit key" << std::endl;
    std::cout << std::endl;
}

/*
message GetKNNRequest {
    string table = 1;
    string key = 2;
    int32 limit = 3;
    string index_name = 4;
}

message VecDt {
    string key = 1;
    double distance = 2;
    string attach_value1 = 3;
    string attach_value2 = 4;
    string attach_value3 = 5;
}

message GetKNNReply {
    int32 code = 1;
    string msg = 2;
    repeated VecDt vecdts = 3;
}
*/

int main(int argc, char **argv) {
    vectordb::Status s;
    exe_name = std::string(argv[0]);

    if (argc != 6) {
        Usage();
        exit(-1);
    }

    std::string address, table_name, index_name, key;
    int limit;
    address = argv[1];
    table_name = argv[2];
    index_name = argv[3];
    key = argv[5];
    sscanf(argv[4], "%d", &limit);

    vectordb::VdbClient vdb_client(address);
    s = vdb_client.Connect();
    assert(s.ok());

    vectordb_rpc::GetKNNRequest request;
    request.set_table(table_name);
    request.set_key(key);
    request.set_limit(limit);
    request.set_index_name(index_name);

    vectordb_rpc::GetKNNReply reply;
    s = vdb_client.GetKNN(request, &reply);
    if (s.ok()) {
        //std::cout << reply.DebugString();
        for (int i = 0; i < reply.vecdts_size(); ++i) {
            std::string key = reply.vecdts(i).key();
            double distance = reply.vecdts(i).distance();
            std::string attach_value1 = reply.vecdts(i).attach_value1();
            printf("%s\t%lf\t%s\n", key.c_str(), distance, attach_value1.c_str());
        }

    } else {
        std::cout << s.ToString() << std::endl;
    }

    return 0;
}
