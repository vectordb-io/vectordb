#ifndef __VECTORDB_VINDEX_H__
#define __VECTORDB_VINDEX_H__

#include <string>
#include "status.h"

namespace vectordb {

class VecDt {
  public:
    VecDt(const std::string key, double distance)
        :key_(key), distance_(distance) {
    }

    const std::string& key() const {
        return key_;
    }

    double distance() const {
        return distance_;
    }

    bool operator < (const VecDt &rhs) const {
        return distance_ < rhs.distance_;
    }

  private:
    std::string key_;
    double distance_;
};

class VIndex {
  public:
    VIndex() = default;
    VIndex(const VIndex&) = delete;
    VIndex& operator=(const VIndex&) = delete;
    virtual ~VIndex() = default;

    virtual Status GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) = 0;
    virtual Status GetKNN(const Vec &vec, int limit, std::vector<VecDt> &results) = 0;
    virtual Status Distance(const std::string &key1, const std::string &key2, double &distance) = 0;
    virtual Status Init() = 0;
};

} // namespace vectordb

#endif
