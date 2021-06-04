#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <functional>
#include <glog/logging.h>
#include "status.h"
#include "config.h"
#include "server.h"

std::string exe_name;

void PrintHelp() {
    std::cout << std::endl;
    std::cout << "Usage: " << std::endl;

    std::cout << std::endl;
    std::cout << exe_name << " --addr=127.0.0.1:38000 --data_path=/tmp/vectordb" << std::endl;
    std::cout << exe_name << " -h" << std::endl;
    std::cout << exe_name << " --help" << std::endl;
    std::cout << exe_name << " -v" << std::endl;
    std::cout << exe_name << " --version" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char** argv) {
    vectordb::Status s;
    FLAGS_alsologtostderr = true;
    google::InitGoogleLogging(argv[0]);
    exe_name = std::string(argv[0]);

    if (argc < 2) {
        PrintHelp();
        exit(0);
    }

    s = vectordb::Config::GetInstance().Load(argc, argv);
    if (s.ok()) {
        LOG(INFO) << "read config: \n" << vectordb::Config::GetInstance().ToString();
    } else {
        PrintHelp();
        exit(-1);
    }

    s = vectordb::Server::GetInstance().Init();
    assert(s.ok());

    s = vectordb::Server::GetInstance().Start();
    assert(s.ok());

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    google::ShutdownGoogleLogging();
    return 0;
}
