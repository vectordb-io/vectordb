#ifndef __VECTORDB_VEC_H__
#define __VECTORDB_VEC_H__

#include <string>
#include <vector>

namespace vectordb {

class Vec {
  public:
    Vec();
    Vec(const std::vector<double> &data);
    Vec(const Vec&) = default;
    Vec& operator=(const Vec&) = default;
    ~Vec() = default;

    int dim() const {
        return data_.size();
    }

    const std::vector<double>&
    data() const {
        return data_;
    }

    std::vector<double>&
    mutable_data() {
        return data_;
    }

  private:
    std::vector<double> data_;
};

class VecObj {
  public:
    VecObj() = default;
    VecObj(const VecObj&) = default;
    VecObj& operator=(const VecObj&) = default;
    ~VecObj() = default;

    std::string key() const {
        return key_;
    }

    void set_key(const std::string &key) {
        key_ = key;
    }

    const Vec& vec() const {
        return vec_;
    }

    Vec& mutable_vec() {
        return vec_;
    }

    std::string attach_value1() const {
        return attach_value1_;
    }

    void set_attach_value1(const std::string &attach_value1) {
        attach_value1_ = attach_value1;
    }

    std::string attach_value2() const {
        return attach_value2_;
    }

    void set_attach_value2(const std::string &attach_value2) {
        attach_value2_ = attach_value2;
    }

    std::string attach_value3() const {
        return attach_value3_;
    }

    void set_attach_value3(const std::string &attach_value3) {
        attach_value3_ = attach_value3;
    }

  private:
    std::string key_;
    Vec vec_;
    std::string attach_value1_;
    std::string attach_value2_;
    std::string attach_value3_;
};

} // namespace vectordb

#endif
