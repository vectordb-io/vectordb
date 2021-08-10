#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <glog/logging.h>
#include "status.h"
#include "vengine.h"
#include "vindex_annoy.h"

float random_float(float min, float max) {
    float r = min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
    return r;
}

std::string exe_name;

vectordb::VEngine *g_vengine;

std::string test_key;

void PutVec(int i, int dim) {
    vectordb::VecObj vec_obj;
    for (int i = 0; i < dim; ++i) {
        float d = random_float(-10, 10);
        vec_obj.mutable_vec().mutable_data().push_back(d);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "test_key_%d", i);
    std::string key(buf);
    vec_obj.set_key(key);
    vec_obj.set_attach_value1("test_attach_value1");
    vec_obj.set_attach_value2("test_attach_value2");
    vec_obj.set_attach_value3("test_attach_value3");

    auto s = g_vengine->Put(key, vec_obj);
    assert(s.ok());

    printf("put vec_obj: %s \n\n", vec_obj.ToString().c_str());

    if (i == 0) {
        test_key = key;
    }
}

void GetVec(int i) {
    vectordb::VecObj vec_obj;
    char buf[32];
    snprintf(buf, sizeof(buf), "test_key_%d", i);
    std::string key(buf);

    auto s = g_vengine->Get(key, vec_obj);
    assert(s.ok());

    printf("get vec_obj: %s \n\n", vec_obj.ToString().c_str());
}

void test_index(std::string distance_type, int dim) {
    do {
        vectordb::AnnoyParam annoy_param;
        annoy_param.dim = dim;
        annoy_param.index_type = "annoy";
        annoy_param.distance_type = distance_type;
        annoy_param.replica_name = "test_replica";
        annoy_param.timestamp = time(nullptr);
        annoy_param.tree_num = 10;

        char index_path[512];
        snprintf(index_path, sizeof(index_path), "/tmp/test_vindex_annoy/%s.%lu", annoy_param.index_type.c_str(), annoy_param.timestamp);

        vectordb::VIndexAnnoy annoy_index(index_path, g_vengine, annoy_param);
        auto s = annoy_index.Build();
        if (!s.ok()) {
            std::cout << "index: " << annoy_index.ToString() << " build error: " << s.ToString();
            return;
        }

        printf("%s \n", annoy_index.ToString().c_str());

        {
            std::cout << std::endl << std::endl;
            int limit = 5;
            std::vector<vectordb::VecDt> results;
            printf("getknn by key: %s \n\n", test_key.c_str());
            s = annoy_index.GetKNN(test_key, limit, results);
            assert(s.ok());
            for (size_t i = 0; i < results.size(); ++i) {
                std::cout << "result-" << i << " : " << results[i].ToString() << std::endl;
            }
            std::cout << std::endl << std::endl;
        }

        {
            std::cout << std::endl << std::endl;
            int limit = 5;
            std::vector<vectordb::VecDt> results;
            printf("getknn by vec: \n\n");
            std::vector<float> vec;
            for (int i = 0; i < dim; ++i) {
                vec.push_back(random_float(-10, 10));
            }

            s = annoy_index.GetKNN(vec, limit, results);
            assert(s.ok());
            for (size_t i = 0; i < results.size(); ++i) {
                std::cout << "result-" << i << " : " << results[i].ToString() << std::endl;
            }
            std::cout << std::endl << std::endl;
        }

    } while (0);
}

int main(int argc, char** argv) {
    vectordb::Status s;
    exe_name = std::string(argv[0]);

    FLAGS_alsologtostderr = true;
    FLAGS_logbufsecs = 0;
    FLAGS_max_log_size = 10;
    google::InitGoogleLogging(argv[0]);

    int dim = 10;

    vectordb::VEngineParam param;
    param.dim = dim;
    param.replica_name = "test_replica";
    std::string path = "/tmp/test_vindex_annoy/data";

    vectordb::VEngine vengine(path, param);
    g_vengine = &vengine;
    s = vengine.Init();
    if (!s.ok()) {
        printf("%s \n", s.ToString().c_str());
        return -1;
    }
    printf("vengine: %s \n", vengine.ToString().c_str());

    int count = 1000;
    for (int i = 0; i < count; ++i) {
        PutVec(i, dim);
    }

    test_index(VINDEX_DISTANCE_TYPE_COSINE, dim);
    test_index(VINDEX_DISTANCE_TYPE_COSINE, dim);
    test_index(VINDEX_DISTANCE_TYPE_INNER_PRODUCT, dim);
    test_index(VINDEX_DISTANCE_TYPE_INNER_PRODUCT, dim);
    test_index(VINDEX_DISTANCE_TYPE_EUCLIDEAN, dim);
    test_index(VINDEX_DISTANCE_TYPE_EUCLIDEAN, dim);
    test_index("bad_type", dim);



    google::ShutdownGoogleLogging();
    return 0;
}
