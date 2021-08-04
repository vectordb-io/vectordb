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

    return 0;
}
