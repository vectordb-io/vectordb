#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <glog/logging.h>
#include "status.h"
#include "vengine.h"
#include "vindex_annoy.h"

std::string exe_name;
vectordb::VEngine *g_vengine;

float random_float(float min, float max) {
    float r = min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
    return r;
}

bool TravelDir(const std::string &path, std::vector<std::string> &dirs) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return false;
    }

    dirs.clear();
    dirent *direntp;
    while ((direntp = readdir(dir)) != nullptr) {
        if (std::string(direntp->d_name) == "." || std::string(direntp->d_name) == "..") {
            continue;
        }

        std::string temp_path = path;
        temp_path.append("/").append(direntp->d_name);
        dirs.push_back(temp_path);
        //std::cout << temp_path << std::endl;
    }

    closedir(dir);
    return true;
}

int main(int argc, char** argv) {
    vectordb::Status s;
    exe_name = std::string(argv[0]);

    FLAGS_alsologtostderr = true;
    FLAGS_logbufsecs = 0;
    FLAGS_max_log_size = 10;
    google::InitGoogleLogging(argv[0]);

    std::string path = "/tmp/test_vindex_annoy/data";
    vectordb::VEngine vengine(path);
    g_vengine = &vengine;
    s = vengine.Load();
    if (!s.ok()) {
        printf("%s \n", s.ToString().c_str());
        return -1;
    }
    printf("vengine: %s \n", vengine.ToString().c_str());

    std::string index_path = "/tmp/test_vindex_annoy/index";
    std::vector<vectordb::VIndexAnnoy*> indices;
    std::vector<std::string> dirs;
    std::cout << "TravelDir " << index_path << std::endl;
    auto b = TravelDir(index_path, dirs);
    assert(b);
    for (auto &p: dirs) {
        vectordb::VIndexAnnoy *annoy_index = new vectordb::VIndexAnnoy(p, g_vengine);
        auto s = annoy_index->Load();
        assert(s.ok());
        indices.push_back(annoy_index);
    }

    std::cout << "load index ok: " << std::endl << std::endl;
    for (auto &index : indices) {
        std::cout << index->ToString() << std::endl;

        // getknn
        {
            std::cout << std::endl << std::endl;
            int limit = 5;
            std::vector<vectordb::VecDt> results;
            printf("getknn by vec: \n\n");
            std::vector<float> vec;
            for (int j = 0; j < index->dim(); ++j) {
                vec.push_back(random_float(-10, 10));
            }

            s = index->GetKNN(vec, limit, results);
            if (!s.ok()) {
                std::cout << "getknn error, " << s.ToString() << std::endl;
            }
            for (size_t j = 0; j < results.size(); ++j) {
                std::cout << "result-" << j << " : " << results[j].ToString() << std::endl;
            }
            std::cout << std::endl << std::endl;
        }
    }

    google::ShutdownGoogleLogging();
    return 0;
}
