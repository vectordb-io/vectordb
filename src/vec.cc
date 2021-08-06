#include "vec.h"

namespace vectordb {

Vec::Vec() {
}

Vec::Vec(const std::vector<float> &data)
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

void
Vec::SerializeToString(std::string &buf) const {
    vectordb_rpc::Vec pb;
    for (auto &d : data_) {
        pb.add_data(d);
    }
    bool ret = pb.SerializeToString(&buf);
    assert(ret);
}

bool
Vec::ParseFromString(const std::string &buf) {
    vectordb_rpc::Vec pb;
    bool ret = pb.ParseFromString(buf);
    if (ret) {
        data_.clear();
        for (int i = 0; i < pb.data_size(); ++i) {
            data_.push_back(pb.data(i));
        }
    }
    return ret;
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

void
VecObj::SerializeToString(std::string &buf) const {
    vectordb_rpc::Vec pb_vec;
    for (auto &d : vec_.data()) {
        pb_vec.add_data(d);
    }
    vectordb_rpc::VecObj pb_vecobj;
    pb_vecobj.set_key(key_);
    *(pb_vecobj.mutable_vec()) = pb_vec;
    pb_vecobj.set_attach_value1(attach_value1_);
    pb_vecobj.set_attach_value2(attach_value2_);
    pb_vecobj.set_attach_value3(attach_value3_);
}

bool
VecObj::ParseFromString(const std::string &buf) {
    vectordb_rpc::VecObj pb;
    bool ret = pb.ParseFromString(buf);
    if (ret) {
        key_ = pb.key();
        attach_value1_ = pb.attach_value1();
        attach_value2_ = pb.attach_value2();
        attach_value3_ = pb.attach_value3();

        vec_.mutable_data().clear();
        for (int i = 0; i < pb.vec().data_size(); ++i) {
            vec_.mutable_data().push_back(pb.vec().data(i));
        }
    }
    return ret;
}

} // namespace vectordb
