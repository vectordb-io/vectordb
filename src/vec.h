#ifndef __VECTORDB_VEC_H__
#define __VECTORDB_VEC_H__

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

} // namespace vectordb

#endif
