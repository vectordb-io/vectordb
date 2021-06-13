#include "vec.h"

namespace vectordb {

Vec::Vec() {
}

Vec::Vec(const std::vector<double> &data)
    :data_(data) {
}


} // namespace vectordb
