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

    return 0;
}
