#include "vdb.h"

#include <gtest/gtest.h>

#include "util.h"

const std::string kTestDir = "/tmp/vdb_test";

// 测试 Vdb::Version
TEST(VdbTest, Version) {
  fs::remove_all(kTestDir);

  vdb::DBParam param;
  param.set_path(kTestDir);
  param.set_name("test");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  vectordb::Vdb vdb(param);

  EXPECT_EQ(vdb.Version(), "0.0.1");
}

// 测试 Vdb::New
TEST(VdbTest, New) {
  // 首先清理测试目录
  fs::remove_all(kTestDir);

  // 创建 Vdb 实例，这将触发 New 方法
  vdb::DBParam param;
  param.set_path(kTestDir);
  param.set_name("test");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  vectordb::Vdb vdb(param);

  // 验证目录结构是否正确创建
  EXPECT_TRUE(fs::exists(kTestDir));

  // 验证日志文件是否创建
  EXPECT_TRUE(fs::exists(kTestDir + "/vdb.log"));
}

// 测试 Vdb::Load
TEST(VdbTest, Load) {
  // 首先清理测试目录
  fs::remove_all(kTestDir);

  vdb::DBParam param;
  param.set_path(kTestDir);
  param.set_name("test");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  { vectordb::Vdb vdb(param); }

  // 创建 Vdb 实例，这将触发 Load 方法
  vectordb::Vdb vdb(param);

  // 验证目录结构是否存在
  EXPECT_TRUE(fs::exists(kTestDir));

  // 验证日志文件是否创建
  EXPECT_TRUE(fs::exists(kTestDir + "/vdb.log"));
}

// 测试 Vdb::CreateTable
TEST(VdbTest, CreateTable) {
  // 首先清理测试目录
  fs::remove_all(kTestDir);

  // 创建 Vdb 实例
  vdb::DBParam param;
  param.set_path(kTestDir);
  param.set_name("test");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  vectordb::Vdb vdb(param);

  // 测试第一个重载版本 - 使用名称和维度创建表
  std::string table_name1 = "test_table1";
  int32_t dim1 = 128;
  auto ret1 = vdb.CreateTable(table_name1, dim1);
  EXPECT_EQ(ret1, vectordb::RET_OK);

  // 测试第二个重载版本 - 使用名称和索引信息创建表
  std::string table_name2 = "test_table2";
  vdb::IndexInfo index_info;
  index_info.set_index_type(vectordb::INDEX_TYPE_HNSW);

  // 创建 HNSW 参数
  vdb::HnswParam* hnsw_param = index_info.mutable_hnsw_param();
  hnsw_param->set_dim(256);
  hnsw_param->set_distance_type(vectordb::DISTANCE_TYPE_L2);
  hnsw_param->set_max_elements(10000);
  hnsw_param->set_m(16);
  hnsw_param->set_ef_construction(200);

  auto ret2 = vdb.CreateTable(table_name2, index_info);
  EXPECT_EQ(ret2, vectordb::RET_OK);

  // 验证表是否成功创建 - 可以通过检查表目录是否存在
  EXPECT_TRUE(fs::exists(kTestDir + "/" + table_name1));
  EXPECT_TRUE(fs::exists(kTestDir + "/" + table_name2));

  // 测试重复创建同名表应该返回错误
  auto ret3 = vdb.CreateTable(table_name1, dim1);
  EXPECT_NE(ret3, vectordb::RET_OK);
}

// 测试 Vdb::Add
TEST(VdbTest, Add) {
  // 首先清理测试目录
  fs::remove_all(kTestDir);

  // 创建 Vdb 实例
  vdb::DBParam param;
  param.set_path(kTestDir);
  param.set_name("test");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  vectordb::Vdb vdb(param);

  // 创建一个表
  std::string table_name = "test_table";
  int32_t dim = 4;
  auto ret = vdb.CreateTable(table_name, dim);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 测试第一个重载版本 - 添加带有标量数据的向量
  int64_t id1 = 1;
  std::vector<float> vector1 = {1.0f, 2.0f, 3.0f, 4.0f};
  std::string scalar1 = "{\"name\":\"vector1\",\"tag\":\"test\"}";
  ret = vdb.Add(table_name, id1, vector1, scalar1);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 测试第二个重载版本 - 只添加向量数据
  int64_t id2 = 2;
  std::vector<float> vector2 = {5.0f, 6.0f, 7.0f, 8.0f};
  ret = vdb.Add(table_name, id2, vector2);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 验证添加是否成功 - 通过 Get 方法获取并验证
  std::vector<float> result_vector;
  std::string result_scalar;

  // 验证第一个向量
  ret = vdb.Get(table_name, id1, result_vector, result_scalar);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(result_vector.size(), vector1.size());
  for (size_t i = 0; i < vector1.size(); i++) {
    EXPECT_FLOAT_EQ(result_vector[i], vector1[i]);
  }
  EXPECT_EQ(result_scalar, scalar1);

  // 验证第二个向量
  result_vector.clear();
  result_scalar.clear();
  ret = vdb.Get(table_name, id2, result_vector, result_scalar);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(result_vector.size(), vector2.size());
  for (size_t i = 0; i < vector2.size(); i++) {
    EXPECT_FLOAT_EQ(result_vector[i], vector2[i]);
  }
  EXPECT_TRUE(result_scalar.empty());  // 第二个向量没有标量数据

  // 测试添加重复ID
  std::vector<float> vector3 = {9.0f, 10.0f, 11.0f, 12.0f};
  ret = vdb.Add(table_name, id1, vector3);  // 使用已存在的id1
  EXPECT_EQ(ret, vectordb::RET_OK);         // 应该可以覆盖

  // 验证覆盖是否成功
  result_vector.clear();
  ret = vdb.Get(table_name, id1, result_vector);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(result_vector.size(), vector3.size());
  for (size_t i = 0; i < vector3.size(); i++) {
    EXPECT_FLOAT_EQ(result_vector[i], vector3[i]);
  }

  // 测试添加到不存在的表
  std::string non_exist_table = "non_exist_table";
  ret = vdb.Add(non_exist_table, id1, vector1);
  EXPECT_EQ(ret, vectordb::RET_ERROR);
}

// 测试 Vdb::Get
TEST(VdbTest, Get) {
  // 首先清理测试目录
  fs::remove_all(kTestDir);

  // 创建 Vdb 实例
  vdb::DBParam param;
  param.set_path(kTestDir);
  param.set_name("test");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  vectordb::Vdb vdb(param);

  // 创建一个表
  std::string table_name = "test_table";
  int32_t dim = 4;
  auto ret = vdb.CreateTable(table_name, dim);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 添加测试数据
  int64_t id1 = 1;
  std::vector<float> vector1 = {1.0f, 2.0f, 3.0f, 4.0f};
  std::string scalar1 = "{\"name\":\"vector1\",\"tag\":\"test\"}";
  ret = vdb.Add(table_name, id1, vector1, scalar1);
  EXPECT_EQ(ret, vectordb::RET_OK);

  int64_t id2 = 2;
  std::vector<float> vector2 = {5.0f, 6.0f, 7.0f, 8.0f};
  ret = vdb.Add(table_name, id2, vector2);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 测试第一个重载版本 - 获取向量和标量
  std::vector<float> result_vector;
  std::string result_scalar;

  // 测试获取存在的 ID
  ret = vdb.Get(table_name, id1, result_vector, result_scalar);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(result_vector.size(), vector1.size());
  for (size_t i = 0; i < vector1.size(); i++) {
    EXPECT_FLOAT_EQ(result_vector[i], vector1[i]);
  }
  EXPECT_EQ(result_scalar, scalar1);

  // 测试第二个重载版本 - 只获取向量
  result_vector.clear();
  ret = vdb.Get(table_name, id2, result_vector);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(result_vector.size(), vector2.size());
  for (size_t i = 0; i < vector2.size(); i++) {
    EXPECT_FLOAT_EQ(result_vector[i], vector2[i]);
  }

  // 测试第三个重载版本 - 只获取标量
  result_scalar.clear();
  ret = vdb.Get(table_name, id1, result_scalar);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(result_scalar, scalar1);

  // 测试获取不存在的 ID
  int64_t non_exist_id = 999;
  result_vector.clear();
  result_scalar.clear();
  ret = vdb.Get(table_name, non_exist_id, result_vector, result_scalar);
  EXPECT_EQ(ret, vectordb::RET_NOT_FOUND);

  // 测试从不存在的表获取数据
  std::string non_exist_table = "non_exist_table";
  result_vector.clear();
  result_scalar.clear();
  ret = vdb.Get(non_exist_table, id1, result_vector, result_scalar);
  EXPECT_EQ(ret, vectordb::RET_NOT_FOUND);

  // 测试获取没有标量的向量的标量
  result_scalar.clear();
  ret = vdb.Get(table_name, id2, result_scalar);
  EXPECT_EQ(ret, vectordb::RET_NOT_FOUND);
  EXPECT_TRUE(result_scalar.empty());
}

// 测试 Vdb::BuildIndex
TEST(VdbTest, BuildIndex) {
  // 首先清理测试目录
  fs::remove_all(kTestDir);

  // 创建 Vdb 实例
  vdb::DBParam param;
  param.set_path(kTestDir);
  param.set_name("test");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  vectordb::Vdb vdb(param);

  // 创建一个表
  std::string table_name = "test_table";
  int32_t dim = 128;
  auto ret = vdb.CreateTable(table_name, dim);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 添加一些向量数据
  for (int64_t id = 1; id <= 100; id++) {
    std::vector<float> vector(dim, 0.0f);
    // 生成一些随机数据
    for (int i = 0; i < dim; i++) {
      vector[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    ret = vdb.Add(table_name, id, vector);
    EXPECT_EQ(ret, vectordb::RET_OK);
  }

  // 测试第一个重载版本 - 不带参数的 BuildIndex
  ret = vdb.BuildIndex(table_name);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 测试第二个重载版本 - 带 IndexInfo 参数的 BuildIndex
  vdb::IndexInfo index_info;
  index_info.set_index_type(vectordb::INDEX_TYPE_HNSW);

  // 创建 HNSW 参数
  vdb::HnswParam* hnsw_param = index_info.mutable_hnsw_param();
  hnsw_param->set_dim(dim);
  hnsw_param->set_distance_type(vectordb::DISTANCE_TYPE_L2);
  hnsw_param->set_max_elements(10000);
  hnsw_param->set_m(16);
  hnsw_param->set_ef_construction(200);

  ret = vdb.BuildIndex(table_name, index_info);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 测试在不存在的表上构建索引
  std::string non_exist_table = "non_exist_table";
  ret = vdb.BuildIndex(non_exist_table);
  EXPECT_EQ(ret, vectordb::RET_NOT_FOUND);

  // 测试使用索引进行搜索
  std::vector<float> query_vector(dim, 0.0f);
  for (int i = 0; i < dim; i++) {
    query_vector[i] = static_cast<float>(rand()) / RAND_MAX;
  }

  std::vector<int64_t> ids;
  std::vector<float> distances;
  std::vector<std::string> scalars;
  int32_t k = 10;

  ret = vdb.Search(table_name, query_vector, k, ids, distances, scalars);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(ids.size(), static_cast<size_t>(k));
  EXPECT_EQ(distances.size(), static_cast<size_t>(k));
  EXPECT_EQ(scalars.size(), static_cast<size_t>(k));
}

// 测试 Vdb::Search
TEST(VdbTest, Search) {
  // 首先清理测试目录
  fs::remove_all(kTestDir);

  // 创建 Vdb 实例
  vdb::DBParam param;
  param.set_path(kTestDir);
  param.set_name("test");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  vectordb::Vdb vdb(param);

  // 创建一个表
  std::string table_name = "test_table";
  int32_t dim = 8;
  auto ret = vdb.CreateTable(table_name, dim);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 添加一些向量数据，使用有规律的数据便于验证
  std::vector<std::vector<float>> vectors = {
      {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // id 1
      {0.8f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // id 2
      {0.6f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // id 3
      {0.4f, 0.6f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // id 4
      {0.2f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // id 5
      {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // id 6
      {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // id 7
      {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},  // id 8
      {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},  // id 9
      {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},  // id 10
  };

  std::vector<std::string> scalars = {
      "{\"name\":\"vector1\",\"tag\":\"group1\"}",
      "{\"name\":\"vector2\",\"tag\":\"group1\"}",
      "{\"name\":\"vector3\",\"tag\":\"group1\"}",
      "{\"name\":\"vector4\",\"tag\":\"group2\"}",
      "{\"name\":\"vector5\",\"tag\":\"group2\"}",
      "{\"name\":\"vector6\",\"tag\":\"group2\"}",
      "{\"name\":\"vector7\",\"tag\":\"group3\"}",
      "{\"name\":\"vector8\",\"tag\":\"group3\"}",
      "{\"name\":\"vector9\",\"tag\":\"group3\"}",
      "{\"name\":\"vector10\",\"tag\":\"group4\"}",
  };

  for (size_t i = 0; i < vectors.size(); i++) {
    int64_t id = i + 1;
    ret = vdb.Add(table_name, id, vectors[i], scalars[i]);
    EXPECT_EQ(ret, vectordb::RET_OK);
  }

  // 构建索引
  ret = vdb.BuildIndex(table_name);
  EXPECT_EQ(ret, vectordb::RET_OK);

  // 测试第一个重载版本 - 使用查询向量进行搜索
  std::vector<float> query_vector = {0.9f, 0.1f, 0.0f, 0.0f,
                                     0.0f, 0.0f, 0.0f, 0.0f};
  std::vector<int64_t> ids;
  std::vector<float> distances;
  std::vector<std::string> result_scalars;
  int32_t k = 3;

  ret = vdb.Search(table_name, query_vector, k, ids, distances, result_scalars);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(ids.size(), static_cast<size_t>(k));
  EXPECT_EQ(distances.size(), static_cast<size_t>(k));
  EXPECT_EQ(result_scalars.size(), static_cast<size_t>(k));

  // 由于我们使用的是有规律的数据，可以验证搜索结果
  // 查询向量 [0.9f, 0.1f, 0.0f, ...] 最接近 id 1 和 id 2
  EXPECT_TRUE(ids[0] == 1 || ids[0] == 2);

  // 测试第二个重载版本 - 使用向量 ID 进行搜索
  int64_t query_id = 1;  // 使用 id 1 的向量进行搜索
  ids.clear();
  distances.clear();
  result_scalars.clear();

  ret = vdb.Search(table_name, query_id, k, ids, distances, result_scalars);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(ids.size(), static_cast<size_t>(k));
  EXPECT_EQ(distances.size(), static_cast<size_t>(k));
  EXPECT_EQ(result_scalars.size(), static_cast<size_t>(k));

  // id 1 的向量应该与自己最相似，然后是 id 2
  EXPECT_EQ(ids[0], 1);                 // 最相似的应该是自己
  EXPECT_FLOAT_EQ(distances[0], 0.0f);  // 与自己的距离应为0
  EXPECT_TRUE(ids[1] == 2);             // 第二相似的应该是 id 2

  // 测试搜索不存在的向量 ID
  int64_t non_exist_id = 999;
  ids.clear();
  distances.clear();
  result_scalars.clear();

  ret = vdb.Search(table_name, non_exist_id, k, ids, distances, result_scalars);
  EXPECT_EQ(ret, vectordb::RET_NOT_FOUND);

  // 测试在不存在的表上搜索
  std::string non_exist_table = "non_exist_table";
  ids.clear();
  distances.clear();
  result_scalars.clear();

  ret = vdb.Search(non_exist_table, query_vector, k, ids, distances,
                   result_scalars);
  EXPECT_EQ(ret, vectordb::RET_NOT_FOUND);

  // 测试使用选项参数
  ids.clear();
  distances.clear();
  result_scalars.clear();

  vectordb::ROptions options;
  ret = vdb.Search(table_name, query_vector, k, ids, distances, result_scalars,
                   options);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(ids.size(), static_cast<size_t>(k));

  // 测试指定索引 ID 参数
  ids.clear();
  distances.clear();
  result_scalars.clear();

  int32_t index_id = 0;  // 假设第一个索引的 ID 是 0
  ret = vdb.Search(table_name, query_vector, k, ids, distances, result_scalars,
                   options, index_id);
  EXPECT_EQ(ret, vectordb::RET_OK);
  EXPECT_EQ(ids.size(), static_cast<size_t>(k));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}