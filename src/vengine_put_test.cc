#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <glog/logging.h>
#include "status.h"
#include "vengine.h"


float random_float(float min, float max) {
    float r = min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
    return r;
}

std::string exe_name;

vectordb::VEngine *g_vengine;

void PutVec(int i, int dim) {
    vectordb::VecObj vec_obj;
    for (int i = 0; i < dim; ++i) {
        float d = random_float(0, 1);
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
    std::string path = "/tmp/test_vengine";

    vectordb::VEngine vengine(path, param);
    g_vengine = &vengine;
    s = vengine.Init();
    if (!s.ok()) {
        printf("%s \n", s.ToString().c_str());
        return -1;
    }
    printf("vengine: %s \n", vengine.ToString().c_str());

    int count = 20;
    for (int i = 0; i < count; ++i) {
        PutVec(i, dim);
    }

    google::ShutdownGoogleLogging();
    return 0;
}
