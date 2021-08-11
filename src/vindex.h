#ifndef __VECTORDB_VINDEX_H__
#define __VECTORDB_VINDEX_H__

#include <string>
#include "jsonxx/json.hpp"
#include "status.h"
#include "util.h"

namespace vectordb {

#define VINDEX_DISTANCE_TYPE_COSINE "cosine"
#define VINDEX_DISTANCE_TYPE_INNER_PRODUCT "inner_product"
#define VINDEX_DISTANCE_TYPE_EUCLIDEAN "euclidean"

#define VINDEX_TYPE_ANNOY "annoy"
#define VINDEX_TYPE_KNN_GRAPH "knn_graph"
#define VINDEX_TYPE_FAISS "faiss"

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

    bool operator > (const VecDt &rhs) const {
        return distance_ > rhs.distance_;
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

struct VIndexParam {
    int dim;
    std::string index_type;
    std::string distance_type;
    std::string replica_name;
    time_t timestamp;
    std::string name;
    std::string path;
};

class VIndex {
  public:
    VIndex() = default;
    VIndex(const VIndexParam &param)
        :dim_(param.dim),
         index_type_(param.index_type),
         distance_type_(param.distance_type),
         replica_name_(param.replica_name),
         timestamp_(param.timestamp),
         name_(param.name),
         path_(param.path) {
    }

    VIndex(const VIndex&) = delete;
    VIndex& operator=(const VIndex&) = delete;
    virtual ~VIndex() = default;

    virtual Status GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) = 0;
    virtual Status GetKNN(const std::vector<float> &vec, int limit, std::vector<VecDt> &results) = 0;
    virtual Status Distance(const std::string &key1, const std::string &key2, float &distance) = 0;
    virtual Status Build() = 0;
    virtual Status Load() = 0;

    virtual jsonxx::json64 ToJson() const = 0;
    virtual std::string ToString() const = 0;
    virtual std::string ToStringPretty() const = 0;

    int dim() const {
        return dim_;
    }

    std::string index_type() const {
        return index_type_;
    }

    std::string distance_type() const {
        return distance_type_;
    }

    std::string name() const {
        return name_;
    }

    std::string replica_name() const {
        return replica_name_;
    }

    time_t timestamp() const {
        return timestamp_;
    }

    std::string timestamp_str() const {
        std::string s = util::LocalTimeString(timestamp_);
        return s;
    }

    std::string path() const {
        return path_;
    }

  protected:
    int dim_;
    std::string index_type_;
    std::string distance_type_;
    std::string replica_name_;
    time_t timestamp_;
    std::string name_;
    std::string path_;

};

} // namespace vectordb

#endif
