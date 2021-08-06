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

int main(int argc, char **argv) {

    return 0;
}
