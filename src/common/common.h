#ifndef VECTORDB_COMMON_H
#define VECTORDB_COMMON_H

#include <experimental/filesystem>
#include <string>

#include "nlohmann/json.hpp"

namespace fs = std::experimental::filesystem;
using json = nlohmann::json;

namespace vectordb {

enum IndexType { INDEX_TYPE_FLAT = 100, INDEX_TYPE_HNSW };

enum DistanceType {
  DISTANCE_TYPE_L2 = 200,
  DISTANCE_TYPE_INNER_PRODUCT,
};

}  // namespace vectordb

#endif  // VECTORDB_COMMON_H