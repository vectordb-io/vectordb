#ifndef __VECTORVIndex_VINDEX_H__
#define __VECTORVIndex_VINDEX_H__

#include "vec.h"
#include "status.h"

namespace vectordb {

class VecDt {
  public:
    VecDt(const Vec &vec, double distance)
        :vec_(vec), distance_(distance) {
    }

    const Vec& vec() const {
        return vec_;
    }

    double distance() const {
        return distance_;
    }

    bool operator < (const VecDt &rhs) const {
        return distance_ < rhs.distance_;
    }

  private:
    Vec vec_;
    double distance_;
};

class VIndex {
  public:
    VIndex() = default;
    VIndex(const VIndex&) = delete;
    VIndex& operator=(const VIndex&) = delete;
    virtual ~VIndex();

    virtual Status GetKNN(const std::string &key, std::vector<VecDt> &results) = 0;
    virtual Status GetKNN(const Vec &vec, std::vector<VecDt> &results) = 0;
    virtual Status BuildIndex() = 0;
};

} // namespace vectordb

#endif
