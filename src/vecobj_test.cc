#include <cstdio>
#include <ctime>
#include <random>
#include "vec.h"

float random_float(float min, float max) {
    float r = min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
    return r;
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    int dim = 10;
    vectordb::VecObj vec_obj;
    for (int i = 0; i < dim; ++i) {
        float d = random_float(0, 1);
        vec_obj.mutable_vec().mutable_data().push_back(d);
    }
    vec_obj.set_key("test_key");
    vec_obj.set_attach_value1("test_attach_value1");
    vec_obj.set_attach_value2("test_attach_value2");
    vec_obj.set_attach_value3("test_attach_value3");
    printf("vec_obj: %s \n\n", vec_obj.ToString().c_str());

    std::string buf;
    vec_obj.SerializeToString(buf);

    vectordb::VecObj vec_obj2;
    bool b = vec_obj2.ParseFromString(buf);
    assert(b);
    printf("vec_obj2: %s \n\n", vec_obj2.ToString().c_str());

    return 0;
}
