#include <cstdio>
#include <ctime>
#include <random>
#include "vec.h"

double random_double(double min, double max) {
    double r = min + static_cast<double>(rand()) / (static_cast<double>(RAND_MAX / (max - min)));
    return r;
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    std::vector<double> data;
    int dim = 10;
    for (int i = 0; i < dim; ++i) {
        double d = random_double(0, 1);
        data.push_back(d);
    }

    vectordb::Vec v(data);
    printf("vec: %s \n", v.ToString().c_str());
    printf("\n");

    vectordb::VecObj vec_obj;
    for (int i = 0; i < dim; ++i) {
        double d = random_double(0, 1);
        vec_obj.mutable_vec().mutable_data().push_back(d);
    }
    vec_obj.set_key("test_key");
    vec_obj.set_attach_value1("test_attach_value1");
    vec_obj.set_attach_value2("test_attach_value2");
    vec_obj.set_attach_value3("test_attach_value3");
    printf("vec_obj: %s \n", vec_obj.ToString().c_str());

    return 0;
}
