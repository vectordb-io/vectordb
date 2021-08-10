#ifndef __VECTORDB_VINDEX_H__
#define __VECTORDB_VINDEX_H__

#include <string>
#include "jsonxx/json.hpp"
#include "status.h"

namespace vectordb {

#define VINDEX_DISTANCE_TYPE_COSINE "cosine"
#define VINDEX_DISTANCE_TYPE_INNER_PRODUCT "inner_product"
#define VINDEX_DISTANCE_TYPE_EUCLIDEAN "euclidean"

struct VecDtParam {
    std::string key;
    float distance;
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

    float distance() const {
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

    jsonxx::json64 ToJson() const {
        jsonxx::json64 j, jret;
        j["key"] = key_;
        j["distance"] = distance_;
        j["attach_value1"] = attach_value1_;
        j["attach_value2"] = attach_value2_;
        j["attach_value3"] = attach_value3_;
        jret["VecDt"] = j;
        return jret;
    }

    std::string ToString() const {
        return ToJson().dump();
    }

    std::string ToStringPretty() const {
        return ToJson().dump(4, ' ');
    }

  private:
    std::string key_;
    float distance_;
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
    virtual Status Distance(const std::string &key1, const std::string &key2, float &distance) = 0;
    virtual Status Build() = 0;
    virtual Status Load() = 0;
};

} // namespace vectordb

#endif
