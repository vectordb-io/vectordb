#ifndef __VECTORDB_VINDEX_H__
#define __VECTORDB_VINDEX_H__

#include <string>
#include "status.h"

namespace vectordb {

struct VecDtParam {
    std::string key;
    double distance;
    std::string attach_value1;
    std::string attach_value2;
    std::string attach_value3;
};

class VecDt {
  public:
    VecDt(const VecDtParam &param)
        :key_(param.key),
         distance_(param.distance),
         attach_value1_(param.attach_value1),
         attach_value2_(param.attach_value2),
         attach_value3_(param.attach_value3) {
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

    const std::string& attach_value1() const {
        return attach_value1_;
    }

    const std::string& attach_value2() const {
        return attach_value2_;
    }

    const std::string& attach_value3() const {
        return attach_value3_;
    }

  private:
    std::string key_;
    double distance_;
    std::string attach_value1_;
    std::string attach_value2_;
    std::string attach_value3_;
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
