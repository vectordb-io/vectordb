#include <cstdio>
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
    std::cout << exe_name << " address table_name data_file" << std::endl;
    std::cout <<exe_name << " 127.0.0.1:38000 image_table ../example/test_vector1.txt" << std::endl;
    std::cout << std::endl;
}

void Split(const std::string &s, char separator, std::vector<std::string> &sv, const std::string ignore) {
    std::set<char> ignore_chars;
    for (auto c : ignore) {
        ignore_chars.insert(c);
    }

    std::string sub_str;
    for (auto c : s) {
        auto it = ignore_chars.find(c);
        if (it != ignore_chars.end()) {
            continue;
        }

        if (c != separator) {
            sub_str.push_back(c);
        } else {
            if (!sub_str.empty()) {
                sv.push_back(sub_str);
            }
            sub_str.clear();
        }
    }

    if (!sub_str.empty()) {
        sv.push_back(sub_str);
    }
}

int main(int argc, char **argv) {
    vectordb::Status s;
    FLAGS_alsologtostderr = false;
    exe_name = std::string(argv[0]);
    srand(static_cast<unsigned>(time(nullptr)));

    if (argc < 4) {
        Usage();
        exit(-1);
    }

    std::string address, table_name, data_file;
    address = argv[1];
    table_name = argv[2];
    data_file = argv[3];

    vectordb::VdbClient vdb_client(address);
    s = vdb_client.Connect();
    assert(s.ok());

    std::ifstream file;
    file.open(data_file.c_str(), std::ios::in);

    if (!file.is_open()) {
        return -1;
    }

    std::string line;
    while (getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        //std::cout << line << std::endl;

        std::vector<std::string> sv;
        Split(line, ';', sv, "");

        std::string &partition_id = sv[0];
        std::string &person_id = sv[1];
        std::string &image_file_name = sv[2];
        std::string &file_path = sv[3];
        std::string &vector_data = sv[4];

        std::vector<std::string> sv1;
        Split(vector_data, ',', sv1, "[] \t");

        std::vector<double> v;
        for (int i = 0; i < sv1.size(); ++i) {
            double dd;
            sscanf(sv1[i].c_str(), "%lf", &dd);
            //printf("%.24f; ", dd);
            v.push_back(dd);
        }
        //std::cout << std::endl;

        vectordb_rpc::PutVecRequest request;
        request.set_table(table_name);
        request.mutable_vec_obj()->set_attach_value1(file_path);
        request.mutable_vec_obj()->set_key(image_file_name);
        for (auto d : v) {
            request.mutable_vec_obj()->mutable_vec()->add_data(d);
        }

        printf("v.size(): %d ===== \n", v.size());
        std::cout << request.DebugString();

        vectordb_rpc::PutVecReply reply;
        s = vdb_client.PutVec(request, &reply);
        std::cout << "insert " << image_file_name << ", "<< reply.DebugString();
    }

    return 0;
}
