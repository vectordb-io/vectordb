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

    std::vector<float> data;
    int dim = 10;
    for (int i = 0; i < dim; ++i) {
        float d = random_float(0, 1);
        data.push_back(d);
    }

    vectordb::Vec v(data);
    printf("v: %s \n\n", v.ToString().c_str());

    std::string buf;
    v.SerializeToString(buf);

    vectordb::Vec v2;
    bool b = v2.ParseFromString(buf);
    assert(b);
    printf("v2: %s \n\n", v2.ToString().c_str());

    return 0;
}
