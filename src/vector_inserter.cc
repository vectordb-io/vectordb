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
    std::cout <<exe_name << " 127.0.0.1:38000 vector_table 10 100" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char **argv) {

    return 0;
}
