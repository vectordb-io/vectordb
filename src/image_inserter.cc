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
        Split(line, ',', sv, " []\"");

        //std::cout << sv.size() << "; ";
        //for (int i = 0; i < sv.size(); ++i) {
        //    std::cout << i << ":" << sv[i] << "; ";
        //}
        //std::cout << std::endl;

        std::string image_file_name = sv[0];
        std::string image_url = "http://yun.baidu.com/images/" + image_file_name;

        //std::cout << image_file_name << "; ";
        std::vector<double> v;
        for (int i = 1; i < sv.size(); ++i) {
            double dd;
            sscanf(sv[i].c_str(), "%lf", &dd);
            //printf("%.24f; ", dd);
            v.push_back(dd);
        }
        //std::cout << std::endl;

        vectordb_rpc::PutVecRequest request;
        request.set_table(table_name);
        request.mutable_vec_obj()->set_attach_value1(image_url);
        request.mutable_vec_obj()->set_key(image_file_name);
        for (auto d : v) {
            request.mutable_vec_obj()->mutable_vec()->add_data(d);
        }
        vectordb_rpc::PutVecReply reply;
        s = vdb_client.PutVec(request, &reply);
        std::cout << "insert " << image_file_name << ", "<< reply.DebugString();
    }








    /*
        std::ofstream outfile(out_put_file);

        char buf[256];
        for (int i = 0; i < count; ++i) {
            std::string key;
            vectordb_rpc::PutVecRequest request;
            request.set_table(table_name);
            request.mutable_vec_obj()->set_attach_value1("inserter_test_attach_value1");
            request.mutable_vec_obj()->set_attach_value2("inserter_test_attach_value2");
            request.mutable_vec_obj()->set_attach_value3("inserter_test_attach_value3");

            snprintf(buf, sizeof(buf), "key%d_%d", i, rand());
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
    */
    return 0;
}
