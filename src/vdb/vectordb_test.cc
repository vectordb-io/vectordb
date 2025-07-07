#include "vectordb.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "common.h"
#include "retno.h"

const std::string kTestDir = "/tmp/vectordb_test";

// 测试vectordb::New
TEST(VectordbTest, New) {
  // 确保测试目录不存在
  fs::remove_all(kTestDir);

  // 创建Vectordb实例
  std::string db_name = "test_db";
  vectordb::Vectordb db(db_name, kTestDir);

  // 验证目录结构是否正确创建
  EXPECT_TRUE(fs::exists(kTestDir));
  EXPECT_TRUE(fs::exists(kTestDir + "/data"));
  EXPECT_TRUE(fs::exists(kTestDir + "/meta"));
  EXPECT_TRUE(fs::exists(kTestDir + "/log"));
  EXPECT_TRUE(fs::exists(kTestDir + "/log/vectordb.log"));

  // 清理测试目录
  // fs::remove_all(kTestDir);
}

// 测试vectordb::Load
TEST(VectordbTest, Load) {
  // 首先创建一个数据库实例
  std::string db_name = "test_load_db";
  {
    // 确保测试目录不存在
    fs::remove_all(kTestDir);

    // 创建Vectordb实例
    vectordb::Vectordb db(db_name, kTestDir);

    // 验证目录结构是否正确创建
    EXPECT_TRUE(fs::exists(kTestDir));
    EXPECT_TRUE(fs::exists(kTestDir + "/data"));
    EXPECT_TRUE(fs::exists(kTestDir + "/meta"));
    EXPECT_TRUE(fs::exists(kTestDir + "/log"));
  }

  // 重新加载数据库实例
  vectordb::Vectordb loaded_db(db_name, kTestDir);

  // 验证目录结构仍然存在
  EXPECT_TRUE(fs::exists(kTestDir));
  EXPECT_TRUE(fs::exists(kTestDir + "/data"));
  EXPECT_TRUE(fs::exists(kTestDir + "/meta"));
  EXPECT_TRUE(fs::exists(kTestDir + "/log"));

  // 清理测试目录
  // fs::remove_all(kTestDir);
}

// 新建vectordb,创建两个表，每个表写入一些数据，每个表创建3个索引。查询
TEST(VectordbTest, CreateTablesAndSearch) {
  // 确保测试目录不存在
  fs::remove_all(kTestDir);

  // 创建Vectordb实例
  std::string db_name = "test_multi_table_db";
  vectordb::Vectordb db(db_name, kTestDir);

  // 创建两个表
  std::string table1_name = "table1";
  std::string table2_name = "table2";
  int32_t dim = 8;

  EXPECT_EQ(vectordb::RET_OK, db.CreateTable(table1_name, dim));
  EXPECT_EQ(vectordb::RET_OK, db.CreateTable(table2_name, dim));

  // 为表1写入数据
  std::vector<std::vector<float>> vectors1 = {
      {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f},
      {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f},
      {0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f},
      {0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f},
      {0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.1f, 1.2f}};

  std::vector<std::string> scalars1 = {
      "{\"name\":\"item1\",\"category\":\"A\"}",
      "{\"name\":\"item2\",\"category\":\"A\"}",
      "{\"name\":\"item3\",\"category\":\"B\"}",
      "{\"name\":\"item4\",\"category\":\"B\"}",
      "{\"name\":\"item5\",\"category\":\"C\"}"};

  for (size_t i = 0; i < vectors1.size(); i++) {
    EXPECT_EQ(vectordb::RET_OK,
              db.Add(table1_name, i + 1, vectors1[i], scalars1[i]));
  }

  // 为表2写入数据
  std::vector<std::vector<float>> vectors2 = {
      {1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f},
      {1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f},
      {1.2f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f},
      {1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f},
      {1.4f, 1.5f, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f, 2.1f}};

  std::vector<std::string> scalars2 = {
      "{\"name\":\"product1\",\"price\":10.5}",
      "{\"name\":\"product2\",\"price\":20.5}",
      "{\"name\":\"product3\",\"price\":30.5}",
      "{\"name\":\"product4\",\"price\":40.5}",
      "{\"name\":\"product5\",\"price\":50.5}"};

  for (size_t i = 0; i < vectors2.size(); i++) {
    EXPECT_EQ(vectordb::RET_OK,
              db.Add(table2_name, i + 1, vectors2[i], scalars2[i]));
  }

  // 为每个表创建3个索引，使用不同的参数
  for (int i = 0; i < 3; i++) {
    // 表1索引
    vdb::IndexInfo index_info1;
    index_info1.set_index_type(vectordb::INDEX_TYPE_HNSW);
    vdb::HnswParam* hnsw_param1 = index_info1.mutable_hnsw_param();
    hnsw_param1->set_dim(dim);
    hnsw_param1->set_max_elements(100);  // 设置最大元素数量
    hnsw_param1->set_m(8 + i * 2);       // 不同的M值: 8, 10, 12
    hnsw_param1->set_ef_construction(
        100 + i * 50);  // 不同的efConstruction: 100, 150, 200
    hnsw_param1->set_distance_type(vectordb::DISTANCE_TYPE_L2);
    EXPECT_EQ(vectordb::RET_OK, db.BuildIndex(table1_name, index_info1));

    // 表2索引
    vdb::IndexInfo index_info2;
    index_info2.set_index_type(vectordb::INDEX_TYPE_HNSW);
    vdb::HnswParam* hnsw_param2 = index_info2.mutable_hnsw_param();
    hnsw_param2->set_dim(dim);
    hnsw_param2->set_max_elements(100);  // 设置最大元素数量
    hnsw_param2->set_m(8 + i * 2);
    hnsw_param2->set_ef_construction(100 + i * 50);
    hnsw_param2->set_distance_type(
        vectordb::DISTANCE_TYPE_INNER_PRODUCT);  // 内积距离
    EXPECT_EQ(vectordb::RET_OK, db.BuildIndex(table2_name, index_info2));
  }

  // 持久化数据
  EXPECT_EQ(vectordb::RET_OK, db.Persist());

  // 验证每个表创建了3个索引
  auto index_ids1 = db.IndexIDs(table1_name);
  auto index_ids2 = db.IndexIDs(table2_name);
  EXPECT_EQ(size_t(4), index_ids1.size());  // 1个默认索引 + 3个手动创建的索引
  EXPECT_EQ(size_t(4), index_ids2.size());  // 1个默认索引 + 3个手动创建的索引

  // 在表1中进行查询
  std::vector<float> query_vector1 = {0.25f, 0.35f, 0.45f, 0.55f,
                                      0.65f, 0.75f, 0.85f, 0.95f};
  std::vector<int64_t> result_ids1;
  std::vector<float> distances1;
  std::vector<std::string> result_scalars1;
  int32_t k = 3;

  // 使用最新的索引进行查询
  EXPECT_EQ(vectordb::RET_OK,
            db.Search(table1_name, query_vector1, k, result_ids1, distances1,
                      result_scalars1));

  // 验证返回了k个结果
  EXPECT_EQ(size_t(k), result_ids1.size());
  EXPECT_EQ(size_t(k), distances1.size());
  EXPECT_EQ(size_t(k), result_scalars1.size());

  // 在表2中进行查询
  std::vector<float> query_vector2 = {1.25f, 1.35f, 1.45f, 1.55f,
                                      1.65f, 1.75f, 1.85f, 1.95f};
  std::vector<int64_t> result_ids2;
  std::vector<float> distances2;
  std::vector<std::string> result_scalars2;

  // 使用特定索引ID进行查询
  EXPECT_EQ(vectordb::RET_OK,
            db.Search(table2_name, query_vector2, k, result_ids2, distances2,
                      result_scalars2, vectordb::ROptions(), index_ids2[1]));

  // 验证返回了k个结果
  EXPECT_EQ(size_t(k), result_ids2.size());
  EXPECT_EQ(size_t(k), distances2.size());
  EXPECT_EQ(size_t(k), result_scalars2.size());

  // 使用ID进行查询
  std::vector<int64_t> result_ids3;
  std::vector<float> distances3;
  std::vector<std::string> result_scalars3;

  EXPECT_EQ(vectordb::RET_OK, db.Search(table1_name, 2, k, result_ids3,
                                        distances3, result_scalars3));

  // 验证返回了k个结果
  EXPECT_EQ(size_t(k), result_ids3.size());
  EXPECT_EQ(size_t(k), distances3.size());
  EXPECT_EQ(size_t(k), result_scalars3.size());

  // 清理测试目录
  fs::remove_all(kTestDir);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}