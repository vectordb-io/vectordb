#include <random>
#include <string>
#include <iostream>
#include <glog/logging.h>
#include "status.h"
#include "vclient.h"
#include "cli_config.h"

std::string exe_name;

void PrintHelp() {
    std::cout << std::endl;
    std::cout << "Usage: " << std::endl;

    std::cout << std::endl;
    std::cout << exe_name << " --addr=127.0.0.1:38000" << std::endl;
    std::cout << exe_name << " -h" << std::endl;
    std::cout << exe_name << " --help" << std::endl;
    std::cout << exe_name << " -v" << std::endl;
    std::cout << exe_name << " --version" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char **argv) {
    vectordb::Status s;
    FLAGS_alsologtostderr = false;
    google::InitGoogleLogging(argv[0]);
    exe_name = std::string(argv[0]);

    if (argc < 2) {
        PrintHelp();
        exit(0);
    }

    s = vectordb::CliConfig::GetInstance().Load(argc, argv);
    if (!s.ok()) {
        PrintHelp();
        exit(-1);
    }

    s = vectordb::VClient::GetInstance().Init();
    assert(s.ok());

    s = vectordb::VClient::GetInstance().Start();
    assert(s.ok());

    return 0;
}
