#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <glog/logging.h>
#include "status.h"
#include "vengine.h"

std::string exe_name;

int main(int argc, char** argv) {
    vectordb::Status s;
    exe_name = std::string(argv[0]);

    FLAGS_alsologtostderr = true;
    FLAGS_logbufsecs = 0;
    FLAGS_max_log_size = 10;
    google::InitGoogleLogging(argv[0]);

    vectordb::VEngineParam param;
    param.dim = 10;
    param.replica_name = "test_replica";
    std::string path = "/tmp/test_vengine";

    vectordb::VEngine vengine(path, param);
    s = vengine.Init();
    if (!s.ok()) {
        printf("%s \n", s.ToString().c_str());
        return -1;
    }
    printf("vengine: %s \n", vengine.ToString().c_str());

    google::ShutdownGoogleLogging();
    return 0;
}
