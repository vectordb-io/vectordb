#ifndef VECTORDB_VINDEX_H_
#define VECTORDB_VINDEX_H_

#include <memory>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "vdb_common.h"

namespace vectordb {

enum VIndexType {
  kIndexAnnoy = 0,
  kIndexNum,
};

inline VIndexType U82VIndexType(uint8_t u8) {
  switch (u8) {
    case 0:
      return kIndexAnnoy;
    default:
      assert(0);
  }
}

inline const char *VIndexType2Str(VIndexType index_type) {
  switch (index_type) {
    case kIndexAnnoy:
      return "IndexAnnoy";

    default:
      return "UnknownIndexType";
  }
}

enum DistanceType {
  kCosine = 0,
  kInnerProduct,
  kEuclidean,
};

inline DistanceType U82DistanceType(uint8_t u8) {
  switch (u8) {
    case 0:
      return kCosine;
    case 1:
      return kInnerProduct;
    case 2:
      return kEuclidean;
    default:
      assert(0);
  }
}

inline const char *DistanceType2Str(DistanceType distance_type) {
  switch (distance_type) {
    case kCosine:
      return "Cosine";
    case kInnerProduct:
      return "InnerProduct";
    case kEuclidean:
      return "Euclidean";
    default:
      return "UnknownDistanceType";
  }
}

struct VIndexParam {
  std::string path;
  uint64_t timestamp;
  int32_t dim;
  VIndexType index_type;       // uint8_t
  DistanceType distance_type;  // uint8_t
  int32_t annoy_tree_num;

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

struct VecResult {
  std::string key;
  std::string attach_value;
  float distance;

  bool operator<(const VecResult &rhs) const { return distance < rhs.distance; }
  bool operator>(const VecResult &rhs) const { return distance > rhs.distance; }

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  std::string ToPrintString();
};

struct VecResults {
  std::vector<VecResult> results;

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  std::string ToPrintString();
};

class Vindex {
 public:
  explicit Vindex(const VIndexParam &param);
  virtual ~Vindex();
  Vindex(const Vindex &) = delete;
  Vindex &operator=(const Vindex &) = delete;

  VIndexParam param() const { return param_; }
  void set_param(VIndexParam param) { param_ = param; }

  virtual int32_t GetKNN(const std::string &key,
                         std::vector<VecResult> &results,
                         int limit = DEFAULT_LIMIT) = 0;
  virtual int32_t GetKNN(const std::vector<float> &vec,
                         std::vector<VecResult> &results,
                         int limit = DEFAULT_LIMIT) = 0;

  virtual nlohmann::json ToJson() = 0;
  virtual nlohmann::json ToJsonTiny() = 0;
  virtual std::string ToJsonString(bool tiny, bool one_line) = 0;

 private:
  VIndexParam param_;
};

inline Vindex::Vindex(const VIndexParam &param) : param_(param) {}

inline Vindex::~Vindex() {}

VindexSPtr Create(VIndexType index_type, const std::string &path, VEngineSPtr v,
                  VIndexParam &param);

}  // namespace vectordb

#endif
