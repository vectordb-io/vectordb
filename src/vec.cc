#include "vec.h"

namespace vectordb {

Vec::Vec() {
}

Vec::Vec(const std::vector<double> &data)
    :data_(data) {
}

std::string
Vec::ToString() const {
    jsonxx::json64 j;
    j["dim"] = data_.size();
    for (size_t i = 0; i < data_.size(); ++i) {
        j["data"][i] = data_[i];
    }
    return j.dump();
}

std::string
VecObj::ToString() const {
    jsonxx::json64 j;
    j["key"] = key_;
    j["vec"] = vec_.ToString();
    j["attach_value1"] = attach_value1_;
    j["attach_value2"] = attach_value2_;
    j["attach_value3"] = attach_value3_;
    return j.dump(4, ' ');
}

} // namespace vectordb
