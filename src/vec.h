#ifndef __VECTORDB_VEC_H__
#define __VECTORDB_VEC_H__

#include <vector>

namespace vectordb {

class Vec {
  public:
    Vec();
    Vec(const std::vector<float> &data, int dim);
    Vec(const Vec&) = default;
    Vec& operator=(const Vec&) = default;
    ~Vec() = default;

    int dim() const {
        return dim_;
    }

    void set_dim(int dim) {
        dim_ = dim;
    }

    std::vector<float>&
    data() {
        return data_;
    }

  private:
    std::vector<float> data_;
    int dim_;
};

}  // namespace vectordb

#endif
