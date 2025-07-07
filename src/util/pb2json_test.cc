#include "pb2json.h"

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "vdb.pb.h"

namespace vectordb {

TEST(Pb2JsonTest, FlatParamToJson) {
  vdb::FlatParam param;
  param.set_dim(128);
  param.set_max_elements(10000);
  param.set_distance_type(DISTANCE_TYPE_L2);

  json j = FlatParamToJson(param);

  EXPECT_EQ(j["dim"], 128);
  EXPECT_EQ(j["max_elements"], 10000);
  EXPECT_EQ(j["distance_type"], DISTANCE_TYPE_L2);
}

TEST(Pb2JsonTest, HnswParamToJson) {
  vdb::HnswParam param;
  param.set_dim(256);
  param.set_max_elements(5000);
  param.set_m(16);
  param.set_ef_construction(200);
  param.set_distance_type(DISTANCE_TYPE_INNER_PRODUCT);

  json j = HnswParamToJson(param);

  EXPECT_EQ(j["dim"], 256);
  EXPECT_EQ(j["max_elements"], 5000);
  EXPECT_EQ(j["M"], 16);
  EXPECT_EQ(j["ef_construction"], 200);
  EXPECT_EQ(j["distance_type"], DISTANCE_TYPE_INNER_PRODUCT);
}

TEST(Pb2JsonTest, IndexInfoToJson_FlatParam) {
  vdb::IndexInfo info;
  info.set_index_type(INDEX_TYPE_FLAT);
  auto* flat_param = info.mutable_flat_param();
  flat_param->set_dim(128);
  flat_param->set_max_elements(10000);
  flat_param->set_distance_type(DISTANCE_TYPE_L2);

  json j = IndexInfoToJson(info);

  EXPECT_EQ(j["index_type"], INDEX_TYPE_FLAT);
  EXPECT_TRUE(j.contains("flat_param"));
  EXPECT_FALSE(j.contains("hnsw_param"));
  EXPECT_EQ(j["flat_param"]["dim"], 128);
  EXPECT_EQ(j["flat_param"]["max_elements"], 10000);
  EXPECT_EQ(j["flat_param"]["distance_type"], DISTANCE_TYPE_L2);
}

TEST(Pb2JsonTest, IndexInfoToJson_HnswParam) {
  vdb::IndexInfo info;
  info.set_index_type(INDEX_TYPE_HNSW);
  auto* hnsw_param = info.mutable_hnsw_param();
  hnsw_param->set_dim(256);
  hnsw_param->set_max_elements(5000);
  hnsw_param->set_m(16);
  hnsw_param->set_ef_construction(200);
  hnsw_param->set_distance_type(DISTANCE_TYPE_INNER_PRODUCT);

  json j = IndexInfoToJson(info);

  EXPECT_EQ(j["index_type"], INDEX_TYPE_HNSW);
  EXPECT_FALSE(j.contains("flat_param"));
  EXPECT_TRUE(j.contains("hnsw_param"));
  EXPECT_EQ(j["hnsw_param"]["dim"], 256);
  EXPECT_EQ(j["hnsw_param"]["max_elements"], 5000);
  EXPECT_EQ(j["hnsw_param"]["M"], 16);
  EXPECT_EQ(j["hnsw_param"]["ef_construction"], 200);
  EXPECT_EQ(j["hnsw_param"]["distance_type"], DISTANCE_TYPE_INNER_PRODUCT);
}

TEST(Pb2JsonTest, IndexParamToJson) {
  vdb::IndexParam param;
  param.set_path("/path/to/index");
  param.set_id(42);
  param.set_create_time(1234567890);

  auto* info = param.mutable_index_info();
  info->set_index_type(INDEX_TYPE_FLAT);
  auto* flat_param = info->mutable_flat_param();
  flat_param->set_dim(128);
  flat_param->set_max_elements(10000);
  flat_param->set_distance_type(DISTANCE_TYPE_L2);

  json j = IndexParamToJson(param);

  EXPECT_EQ(j["path"], "/path/to/index");
  EXPECT_EQ(j["id"], 42);
  EXPECT_EQ(j["create_time"], 1234567890);
  EXPECT_TRUE(j.contains("index_info"));
  EXPECT_EQ(j["index_info"]["index_type"], INDEX_TYPE_FLAT);
}

TEST(Pb2JsonTest, TableInfoToJson) {
  vdb::TableInfo info;
  info.set_name("test_table");

  auto* default_index_info = info.mutable_default_index_info();
  default_index_info->set_index_type(INDEX_TYPE_HNSW);
  auto* hnsw_param = default_index_info->mutable_hnsw_param();
  hnsw_param->set_dim(256);
  hnsw_param->set_max_elements(5000);
  hnsw_param->set_m(16);
  hnsw_param->set_ef_construction(200);
  hnsw_param->set_distance_type(DISTANCE_TYPE_INNER_PRODUCT);

  json j = TableInfoToJson(info);

  EXPECT_EQ(j["name"], "test_table");
  EXPECT_TRUE(j.contains("default_index_info"));
  EXPECT_EQ(j["default_index_info"]["index_type"], INDEX_TYPE_HNSW);
}

TEST(Pb2JsonTest, TableParamToJson) {
  vdb::TableParam param;
  param.set_path("/path/to/table");
  param.set_name("test_table");
  param.set_create_time(1234567890);
  param.set_dim(128);

  // 设置默认索引信息
  auto* default_index_info = param.mutable_default_index_info();
  default_index_info->set_index_type(INDEX_TYPE_FLAT);
  auto* flat_param = default_index_info->mutable_flat_param();
  flat_param->set_dim(128);
  flat_param->set_max_elements(10000);
  flat_param->set_distance_type(DISTANCE_TYPE_L2);

  // 添加两个索引
  auto* index1 = param.add_indexes();
  index1->set_path("/path/to/index1");
  index1->set_id(1);
  index1->set_create_time(1234567891);
  auto* index1_info = index1->mutable_index_info();
  index1_info->set_index_type(INDEX_TYPE_FLAT);
  auto* index1_flat_param = index1_info->mutable_flat_param();
  index1_flat_param->set_dim(128);
  index1_flat_param->set_max_elements(10000);
  index1_flat_param->set_distance_type(DISTANCE_TYPE_L2);

  auto* index2 = param.add_indexes();
  index2->set_path("/path/to/index2");
  index2->set_id(2);
  index2->set_create_time(1234567892);
  auto* index2_info = index2->mutable_index_info();
  index2_info->set_index_type(INDEX_TYPE_HNSW);
  auto* index2_hnsw_param = index2_info->mutable_hnsw_param();
  index2_hnsw_param->set_dim(128);
  index2_hnsw_param->set_max_elements(5000);
  index2_hnsw_param->set_m(16);
  index2_hnsw_param->set_ef_construction(200);
  index2_hnsw_param->set_distance_type(DISTANCE_TYPE_INNER_PRODUCT);

  json j = TableParamToJson(param);

  EXPECT_EQ(j["path"], "/path/to/table");
  EXPECT_EQ(j["name"], "test_table");
  EXPECT_EQ(j["create_time"], 1234567890);
  EXPECT_EQ(j["dim"], 128);
  EXPECT_TRUE(j.contains("default_index_info"));
  EXPECT_TRUE(j.contains("indexes"));
  EXPECT_EQ(j["indexes"].size(), static_cast<size_t>(2));
  EXPECT_EQ(j["indexes"][0]["id"], 1);
  EXPECT_EQ(j["indexes"][1]["id"], 2);
}

}  // namespace vectordb