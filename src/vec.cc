#include "vec.h"

namespace vectordb {

Vec::Vec()
    :dim_(0) {
}

Vec::Vec(const std::vector<float> &data, int dim)
    :data_(data), dim_(dim) {
}


}  // namespace vectordb
