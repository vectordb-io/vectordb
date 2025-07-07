#include <gtest/gtest.h>

#include <string>

#include "common.h"
#include "util.h"
#include "vdb.pb.h"

// 测试 message FlatParam 序列化,反序列化
TEST(VdbProtoTest, FlatParamSerializeDeserialize) {
  // 创建 FlatParam 对象并设置参数
  vdb::FlatParam flat_param;
  flat_param.set_dim(42);
  flat_param.set_max_elements(1000);
  flat_param.set_distance_type(vectordb::DISTANCE_TYPE_L2);
  // 序列化到字符串
  std::string serialized;
  ASSERT_TRUE(flat_param.SerializeToString(&serialized));

  // 反序列化
  vdb::FlatParam deserialized_param;
  ASSERT_TRUE(deserialized_param.ParseFromString(serialized));

  // 验证反序列化后的值与原始值相同
  EXPECT_EQ(flat_param.dim(), deserialized_param.dim());
  EXPECT_EQ(flat_param.max_elements(), deserialized_param.max_elements());
  EXPECT_EQ(flat_param.distance_type(), deserialized_param.distance_type());
}

// 测试 message HnswParam 序列化,反序列化
TEST(VdbProtoTest, HnswParamSerializeDeserialize) {
  // 创建 HnswParam 对象并设置参数
  vdb::HnswParam hnsw_param;
  hnsw_param.set_dim(128);
  hnsw_param.set_max_elements(1000);
  hnsw_param.set_m(16);
  hnsw_param.set_ef_construction(100);
  hnsw_param.set_distance_type(vectordb::DISTANCE_TYPE_L2);

  // 序列化到字符串
  std::string serialized;
  ASSERT_TRUE(hnsw_param.SerializeToString(&serialized));

  // 反序列化
  vdb::HnswParam deserialized_param;
  ASSERT_TRUE(deserialized_param.ParseFromString(serialized));

  // 验证反序列化后的值与原始值相同
  EXPECT_EQ(hnsw_param.dim(), deserialized_param.dim());
  EXPECT_EQ(hnsw_param.max_elements(), deserialized_param.max_elements());
  EXPECT_EQ(hnsw_param.m(), deserialized_param.m());
  EXPECT_EQ(hnsw_param.ef_construction(), deserialized_param.ef_construction());
  EXPECT_EQ(hnsw_param.distance_type(), deserialized_param.distance_type());
}

// 测试 message IndexParam 序列化,反序列化
TEST(VdbProtoTest, IndexParamSerializeDeserialize) {
  // 创建 IndexParam 对象并设置参数
  vdb::IndexParam index_param;
  index_param.set_path("test_path");
  index_param.set_id(1);
  index_param.set_create_time(1234567890);

  // 设置 index_info
  vdb::IndexInfo* index_info = index_param.mutable_index_info();
  index_info->set_index_type(vectordb::INDEX_TYPE_FLAT);

  // 设置 flat_param
  vdb::FlatParam* flat_param = index_info->mutable_flat_param();
  flat_param->set_dim(64);
  flat_param->set_max_elements(1000);
  flat_param->set_distance_type(vectordb::DISTANCE_TYPE_L2);

  // 序列化到字符串
  std::string serialized;
  ASSERT_TRUE(index_param.SerializeToString(&serialized));

  // 反序列化
  vdb::IndexParam deserialized_param;
  ASSERT_TRUE(deserialized_param.ParseFromString(serialized));

  // 验证反序列化后的值与原始值相同
  EXPECT_EQ(index_param.path(), deserialized_param.path());
  EXPECT_EQ(index_param.id(), deserialized_param.id());
  EXPECT_EQ(index_param.create_time(), deserialized_param.create_time());
  EXPECT_EQ(index_param.index_info().index_type(),
            deserialized_param.index_info().index_type());
  EXPECT_EQ(index_param.index_info().flat_param().dim(),
            deserialized_param.index_info().flat_param().dim());
  EXPECT_EQ(index_param.index_info().flat_param().max_elements(),
            deserialized_param.index_info().flat_param().max_elements());
  EXPECT_EQ(index_param.index_info().flat_param().distance_type(),
            deserialized_param.index_info().flat_param().distance_type());
}

// 测试 message TableParam 序列化,反序列化
TEST(VdbProtoTest, TableParamSerializeDeserialize) {
  // 创建 TableParam 对象并设置参数
  vdb::TableParam table_param;
  table_param.set_path("test_table_path");
  table_param.set_name("test_table");
  table_param.set_create_time(1234567890);  // 使用固定时间戳
  table_param.set_dim(128);

  // 设置默认索引信息
  vdb::IndexInfo* default_index_info = table_param.mutable_default_index_info();
  default_index_info->set_index_type(vectordb::INDEX_TYPE_FLAT);

  // 设置默认的 flat_param
  vdb::FlatParam* flat_param = default_index_info->mutable_flat_param();
  flat_param->set_dim(128);
  flat_param->set_max_elements(1000);
  flat_param->set_distance_type(vectordb::DISTANCE_TYPE_L2);

  // 添加一个索引参数
  vdb::IndexParam* index_param = table_param.add_indexes();
  index_param->set_path("test_index_path");
  index_param->set_id(1);
  index_param->set_create_time(1234567891);  // 使用固定时间戳

  // 设置索引的index_info
  vdb::IndexInfo* index_info = index_param->mutable_index_info();
  index_info->set_index_type(vectordb::INDEX_TYPE_HNSW);

  // 为索引参数设置 hnsw_param
  vdb::HnswParam* hnsw_param = index_info->mutable_hnsw_param();
  hnsw_param->set_dim(128);
  hnsw_param->set_max_elements(2000);
  hnsw_param->set_m(16);
  hnsw_param->set_ef_construction(200);
  hnsw_param->set_distance_type(vectordb::DISTANCE_TYPE_L2);

  // 序列化到字符串
  std::string serialized;
  ASSERT_TRUE(table_param.SerializeToString(&serialized));

  // 反序列化
  vdb::TableParam deserialized_param;
  ASSERT_TRUE(deserialized_param.ParseFromString(serialized));

  // 验证反序列化后的基本参数与原始值相同
  EXPECT_EQ(table_param.path(), deserialized_param.path());
  EXPECT_EQ(table_param.name(), deserialized_param.name());
  EXPECT_EQ(table_param.create_time(), deserialized_param.create_time());
  EXPECT_EQ(table_param.dim(), deserialized_param.dim());

  // 验证默认索引参数
  EXPECT_EQ(table_param.default_index_info().index_type(),
            deserialized_param.default_index_info().index_type());

  // 验证默认的 flat_param
  EXPECT_EQ(table_param.default_index_info().flat_param().dim(),
            deserialized_param.default_index_info().flat_param().dim());
  EXPECT_EQ(
      table_param.default_index_info().flat_param().max_elements(),
      deserialized_param.default_index_info().flat_param().max_elements());
  EXPECT_EQ(
      table_param.default_index_info().flat_param().distance_type(),
      deserialized_param.default_index_info().flat_param().distance_type());

  // 验证索引参数列表
  ASSERT_EQ(table_param.indexes_size(), deserialized_param.indexes_size());
  EXPECT_EQ(table_param.indexes(0).path(),
            deserialized_param.indexes(0).path());
  EXPECT_EQ(table_param.indexes(0).id(), deserialized_param.indexes(0).id());
  EXPECT_EQ(table_param.indexes(0).create_time(),
            deserialized_param.indexes(0).create_time());

  // 验证索引的index_info
  EXPECT_EQ(table_param.indexes(0).index_info().index_type(),
            deserialized_param.indexes(0).index_info().index_type());

  // 验证索引的 hnsw_param
  EXPECT_EQ(table_param.indexes(0).index_info().hnsw_param().dim(),
            deserialized_param.indexes(0).index_info().hnsw_param().dim());
  EXPECT_EQ(
      table_param.indexes(0).index_info().hnsw_param().max_elements(),
      deserialized_param.indexes(0).index_info().hnsw_param().max_elements());
  EXPECT_EQ(table_param.indexes(0).index_info().hnsw_param().m(),
            deserialized_param.indexes(0).index_info().hnsw_param().m());
  EXPECT_EQ(table_param.indexes(0).index_info().hnsw_param().ef_construction(),
            deserialized_param.indexes(0)
                .index_info()
                .hnsw_param()
                .ef_construction());
  EXPECT_EQ(
      table_param.indexes(0).index_info().hnsw_param().distance_type(),
      deserialized_param.indexes(0).index_info().hnsw_param().distance_type());
}

// 测试 message Vec 序列化,反序列化
TEST(VdbProtoTest, VecSerializeDeserialize) {
  // 创建 Vec 对象并设置数据
  vdb::Vec vec;
  for (int i = 0; i < 5; i++) {
    vec.add_data(i * 1.5);
  }

  // 序列化到字符串
  std::string serialized;
  ASSERT_TRUE(vec.SerializeToString(&serialized));

  // 反序列化
  vdb::Vec deserialized_vec;
  ASSERT_TRUE(deserialized_vec.ParseFromString(serialized));

  // 验证反序列化后的值与原始值相同
  ASSERT_EQ(vec.data_size(), deserialized_vec.data_size());
  for (int i = 0; i < vec.data_size(); i++) {
    EXPECT_FLOAT_EQ(vec.data(i), deserialized_vec.data(i));
  }
}
