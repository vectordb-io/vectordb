#include "pb2json.h"

namespace vectordb {

json FlatParamToJson(const vdb::FlatParam &param) {
  json j;
  j["dim"] = param.dim();
  j["max_elements"] = param.max_elements();
  j["distance_type"] = param.distance_type();
  return j;
}

json HnswParamToJson(const vdb::HnswParam &param) {
  json j;
  j["dim"] = param.dim();
  j["max_elements"] = param.max_elements();
  j["M"] = param.m();
  j["ef_construction"] = param.ef_construction();
  j["distance_type"] = param.distance_type();
  return j;
}

json IndexInfoToJson(const vdb::IndexInfo &param) {
  json j;
  j["index_type"] = param.index_type();
  if (param.has_flat_param()) {
    j["flat_param"] = FlatParamToJson(param.flat_param());
  } else if (param.has_hnsw_param()) {
    j["hnsw_param"] = HnswParamToJson(param.hnsw_param());
  }
  return j;
}

json IndexParamToJson(const vdb::IndexParam &param) {
  json j;
  j["path"] = param.path();
  j["id"] = param.id();
  j["create_time"] = param.create_time();
  j["index_info"] = IndexInfoToJson(param.index_info());
  return j;
}

json TableInfoToJson(const vdb::TableInfo &param) {
  json j;
  j["name"] = param.name();
  j["default_index_info"] = IndexInfoToJson(param.default_index_info());
  return j;
}

json TableParamToJson(const vdb::TableParam &param) {
  json j;
  j["path"] = param.path();
  j["name"] = param.name();
  j["create_time"] = param.create_time();
  j["dim"] = param.dim();
  j["default_index_info"] = IndexInfoToJson(param.default_index_info());
  j["indexes"] = json::array();
  for (const auto &index : param.indexes()) {
    j["indexes"].push_back(IndexParamToJson(index));
  }
  return j;
}

}  // namespace vectordb