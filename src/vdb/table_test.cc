#include "table.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "common.h"
#include "util.h"
#include "vdb.pb.h"

const std::string kTestDir = "/tmp/table_test";

// 测试 Table::New
TEST(TableTest, New) {
  // 清理测试目录
  fs::remove_all(kTestDir);

  // 创建表参数
  vdb::TableParam param;
  param.set_path(kTestDir);
  param.set_name("test_table");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.set_dim(128);
  param.mutable_default_index_info()->set_index_type(
      vectordb::kDefaultIndexType);
  vdb::HnswParam hnsw_param = vectordb::DefaultHnswParam(128);
  param.mutable_default_index_info()->mutable_hnsw_param()->CopyFrom(
      hnsw_param);

  // 创建表
  vectordb::Table table(param);

  // 验证目录结构
  EXPECT_TRUE(fs::exists(kTestDir));
  EXPECT_TRUE(fs::exists(kTestDir + "/data"));
  EXPECT_TRUE(fs::exists(kTestDir + "/index"));
  EXPECT_TRUE(fs::exists(kTestDir + "/description.json"));

  // 验证表参数
  EXPECT_EQ(table.param().path(), kTestDir);
  EXPECT_EQ(table.param().dim(), 128);
  EXPECT_EQ(table.param().name(), "test_table");
}

// 测试 Table::Load
TEST(TableTest, Load) {
  std::vector<float> vector1(128, 0.1f);
  std::vector<float> vector2(128, 0.2f);
  std::vector<float> vector3(128, 0.3f);

  // 确保测试目录存在
  if (fs::exists(kTestDir)) {
    fs::remove_all(kTestDir);
  }

  {
    // 创建表参数
    vdb::TableParam param;
    param.set_path(kTestDir);
    param.set_name("test_table");
    param.set_create_time(vectordb::TimeStamp().MilliSeconds());
    param.set_dim(128);
    param.mutable_default_index_info()->set_index_type(
        vectordb::kDefaultIndexType);
    vdb::HnswParam hnsw_param = vectordb::DefaultHnswParam(128);
    param.mutable_default_index_info()->mutable_hnsw_param()->CopyFrom(
        hnsw_param);

    // 创建表
    vectordb::Table table(param);

    // 添加一些向量数据
    EXPECT_EQ(vectordb::RET_OK, table.Add(1, vector1));
    EXPECT_EQ(vectordb::RET_OK, table.Add(2, vector2));
    EXPECT_EQ(vectordb::RET_OK, table.Add(3, vector3));

    std::vector<float> v;
    vectordb::RetNo ret = table.Get(1, v);
    EXPECT_EQ(ret, vectordb::RET_OK);
    EXPECT_EQ(v, vector1);
    ret = table.Get(2, v);
    EXPECT_EQ(ret, vectordb::RET_OK);
    EXPECT_EQ(v, vector2);
    ret = table.Get(3, v);
    EXPECT_EQ(ret, vectordb::RET_OK);
    EXPECT_EQ(v, vector3);
  }

  // 加载已存在的表
  {
    vdb::TableParam param;
    param.set_path(kTestDir);
    param.set_name("test_table");
    param.set_create_time(vectordb::TimeStamp().MilliSeconds());
    param.set_dim(128);
    param.mutable_default_index_info()->set_index_type(
        vectordb::kDefaultIndexType);
    vdb::HnswParam hnsw_param = vectordb::DefaultHnswParam(128);
    param.mutable_default_index_info()->mutable_hnsw_param()->CopyFrom(
        hnsw_param);
    vectordb::Table loaded_table(param);

    std::vector<float> v;
    vectordb::RetNo ret = loaded_table.Get(1, v);
    EXPECT_EQ(ret, vectordb::RET_OK);
    EXPECT_EQ(v, vector1);
    ret = loaded_table.Get(2, v);
    EXPECT_EQ(ret, vectordb::RET_OK);
    EXPECT_EQ(v, vector2);
    ret = loaded_table.Get(3, v);
    EXPECT_EQ(ret, vectordb::RET_OK);
    EXPECT_EQ(v, vector3);
  }
}

// 测试 Table::BuildIndex();
TEST(TableTest, BuildIndex) {
  // 清理测试目录
  fs::remove_all(kTestDir);

  // 创建表参数
  vdb::TableParam param;
  param.set_path(kTestDir);
  param.set_name("test_table");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.set_dim(128);
  param.mutable_default_index_info()->set_index_type(
      vectordb::kDefaultIndexType);
  vdb::HnswParam hnsw_param = vectordb::DefaultHnswParam(128);
  param.mutable_default_index_info()->mutable_hnsw_param()->CopyFrom(
      hnsw_param);

  // 创建表
  vectordb::Table table(param);

  // 添加一些向量数据
  std::vector<float> vector1(128, 0.1f);
  std::vector<float> vector2(128, 0.2f);
  std::vector<float> vector3(128, 0.3f);

  vectordb::WOptions options;
  options.write_vector_to_data = true;
  options.write_vector_to_index = false;

  EXPECT_EQ(vectordb::RET_OK, table.Add(1, vector1, options, true));
  EXPECT_EQ(vectordb::RET_OK, table.Add(2, vector2, options, true));
  EXPECT_EQ(vectordb::RET_OK, table.Add(3, vector3, options, true));

  EXPECT_TRUE(!fs::exists(kTestDir + "/index/0"));

  // 构建索引
  EXPECT_EQ(vectordb::RET_OK, table.BuildIndex());

  // 验证索引目录是否存在
  EXPECT_TRUE(fs::exists(kTestDir + "/index/0"));

  // 使用索引进行搜索
  std::vector<int64_t> ids;
  std::vector<float> distances;
  std::vector<std::string> scalars;

  // 搜索最相似的2个向量
  EXPECT_EQ(vectordb::RET_OK,
            table.Search(vector1, 2, ids, distances, scalars));

  // 验证搜索结果
  ASSERT_EQ(size_t(2), ids.size());
  ASSERT_EQ(size_t(2), distances.size());
  ASSERT_EQ(size_t(2), scalars.size());

  // 打印搜索结果
  for (size_t i = 0; i < ids.size(); i++) {
    std::cout << "id: " << ids[i] << ", distance: " << distances[i]
              << ", scalar: " << scalars[i] << std::endl;
  }

  EXPECT_EQ(int64_t(1), ids[0]);
}

// 测试 Table::BuildIndex(const vdb::FlatParam &param);
TEST(TableTest, BuildIndexFlat) {
  // 清理测试目录
  fs::remove_all(kTestDir);

  // 创建表参数
  vdb::TableParam param;
  param.set_path(kTestDir);
  param.set_name("test_table");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.set_dim(128);
  param.mutable_default_index_info()->set_index_type(
      vectordb::kDefaultIndexType);
  vdb::HnswParam hnsw_param = vectordb::DefaultHnswParam(128);
  param.mutable_default_index_info()->mutable_hnsw_param()->CopyFrom(
      hnsw_param);

  // 创建表
  vectordb::Table table(param);

  // 添加一些向量数据
  std::vector<float> vector1(128, 0.1f);
  std::vector<float> vector2(128, 0.2f);
  std::vector<float> vector3(128, 0.3f);

  vectordb::WOptions options;
  options.write_vector_to_data = true;
  options.write_vector_to_index = false;

  EXPECT_EQ(vectordb::RET_OK, table.Add(1, vector1, options, true));
  EXPECT_EQ(vectordb::RET_OK, table.Add(2, vector2, options, true));
  EXPECT_EQ(vectordb::RET_OK, table.Add(3, vector3, options, true));

  // 确认索引目录不存在
  EXPECT_TRUE(!fs::exists(kTestDir + "/index/0"));

  // 创建Flat索引参数
  vdb::FlatParam flat_param;
  flat_param.set_dim(128);
  flat_param.set_max_elements(10000);
  flat_param.set_distance_type(vectordb::DISTANCE_TYPE_L2);

  // 使用Flat参数构建索引
  EXPECT_EQ(vectordb::RET_OK, table.BuildIndex(flat_param));

  // 验证索引目录是否存在
  EXPECT_TRUE(fs::exists(kTestDir + "/index/0"));

  // 使用索引进行搜索
  std::vector<int64_t> ids;
  std::vector<float> distances;
  std::vector<std::string> scalars;

  // 搜索最相似的2个向量
  EXPECT_EQ(vectordb::RET_OK,
            table.Search(vector1, 2, ids, distances, scalars));

  // 验证搜索结果
  ASSERT_EQ(size_t(2), ids.size());
  ASSERT_EQ(size_t(2), distances.size());
  ASSERT_EQ(size_t(2), scalars.size());

  // 打印搜索结果
  for (size_t i = 0; i < ids.size(); i++) {
    std::cout << "id: " << ids[i] << ", distance: " << distances[i]
              << ", scalar: " << scalars[i] << std::endl;
  }

  // 由于使用L2距离，向量1应该是最接近自己的
  EXPECT_EQ(int64_t(1), ids[0]);
}

// 测试 Table::BuildIndex(const vdb::HnswParam &param);
TEST(TableTest, BuildIndexHnsw) {
  // 清理测试目录
  fs::remove_all(kTestDir);

  // 创建表参数
  vdb::TableParam param;
  param.set_path(kTestDir);
  param.set_name("test_table");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.set_dim(128);
  param.mutable_default_index_info()->set_index_type(
      vectordb::kDefaultIndexType);
  vdb::HnswParam hnsw_param = vectordb::DefaultHnswParam(128);
  param.mutable_default_index_info()->mutable_hnsw_param()->CopyFrom(
      hnsw_param);

  // 创建表
  vectordb::Table table(param);

  // 添加一些向量数据
  std::vector<float> vector1(128, 0.1f);
  std::vector<float> vector2(128, 0.2f);
  std::vector<float> vector3(128, 0.3f);

  vectordb::WOptions options;
  options.write_vector_to_data = true;
  options.write_vector_to_index = false;

  EXPECT_EQ(vectordb::RET_OK, table.Add(1, vector1, options, true));
  EXPECT_EQ(vectordb::RET_OK, table.Add(2, vector2, options, true));
  EXPECT_EQ(vectordb::RET_OK, table.Add(3, vector3, options, true));

  // 确认索引目录不存在
  EXPECT_TRUE(!fs::exists(kTestDir + "/index/0"));

  // 创建HNSW索引参数
  vdb::HnswParam hnsw_param2;
  hnsw_param2.set_dim(128);
  hnsw_param2.set_max_elements(10000);
  hnsw_param2.set_m(16);                 // 每个节点的最大出度
  hnsw_param2.set_ef_construction(100);  // 构建索引时的搜索深度参数
  hnsw_param2.set_distance_type(vectordb::DISTANCE_TYPE_INNER_PRODUCT);

  // 使用HNSW参数构建索引
  EXPECT_EQ(vectordb::RET_OK, table.BuildIndex(hnsw_param2));

  // 验证索引目录是否存在
  EXPECT_TRUE(fs::exists(kTestDir + "/index/0"));

  // 使用索引进行搜索
  std::vector<int64_t> ids;
  std::vector<float> distances;
  std::vector<std::string> scalars;

  // 搜索最相似的2个向量
  EXPECT_EQ(vectordb::RET_OK,
            table.Search(vector1, 2, ids, distances, scalars));

  // 验证搜索结果
  ASSERT_EQ(size_t(2), ids.size());
  ASSERT_EQ(size_t(2), distances.size());
  ASSERT_EQ(size_t(2), scalars.size());

  // 打印搜索结果
  for (size_t i = 0; i < ids.size(); i++) {
    std::cout << "id: " << ids[i] << ", distance: " << distances[i]
              << ", scalar: " << scalars[i] << std::endl;
  }

  // 由于使用内积距离，向量1应该是最接近自己的
  EXPECT_EQ(int64_t(1), ids[0]);
}

// 创建Table，写入数据，创建两个相同的索引，搜索结果应该相同
TEST(TableTest, MultipleIndices) {
  // 清理测试目录
  fs::remove_all(kTestDir);

  // 创建表参数
  vdb::TableParam param;
  param.set_path(kTestDir);
  param.set_name("test_table");
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.set_dim(128);
  param.mutable_default_index_info()->set_index_type(
      vectordb::kDefaultIndexType);
  vdb::HnswParam hnsw_param = vectordb::DefaultHnswParam(128);
  param.mutable_default_index_info()->mutable_hnsw_param()->CopyFrom(
      hnsw_param);

  // 创建表
  vectordb::Table table(param);

  // 添加一些向量数据
  std::vector<float> vector1(128, 0.1f);
  std::vector<float> vector2(128, 0.2f);
  std::vector<float> vector3(128, 0.3f);
  std::vector<float> query_vector(128, 0.15f);

  vectordb::WOptions options;
  options.write_vector_to_data = true;
  options.write_vector_to_index = false;

  EXPECT_EQ(vectordb::RET_OK, table.Add(1, vector1, options, true));
  EXPECT_EQ(vectordb::RET_OK, table.Add(2, vector2, options, true));
  EXPECT_EQ(vectordb::RET_OK, table.Add(3, vector3, options, true));

  // 创建第一个索引（HNSW）
  vdb::HnswParam hnsw_param2;
  hnsw_param2.set_dim(128);
  hnsw_param2.set_max_elements(10000);
  hnsw_param2.set_m(16);
  hnsw_param2.set_ef_construction(100);
  hnsw_param2.set_distance_type(vectordb::DISTANCE_TYPE_L2);

  EXPECT_EQ(vectordb::RET_OK, table.BuildIndex(hnsw_param2));

  // 使用第一个索引进行搜索
  std::vector<int64_t> ids1;
  std::vector<float> distances1;
  std::vector<std::string> scalars1;

  EXPECT_EQ(vectordb::RET_OK,
            table.Search(query_vector, 3, ids1, distances1, scalars1));

  // 验证搜索结果
  ASSERT_EQ(size_t(3), ids1.size());
  ASSERT_EQ(size_t(3), distances1.size());
  ASSERT_EQ(size_t(3), scalars1.size());

  // 创建第二个相同参数的索引
  vdb::HnswParam hnsw_param3;
  hnsw_param3.set_dim(128);
  hnsw_param3.set_max_elements(10000);
  hnsw_param3.set_m(16);
  hnsw_param3.set_ef_construction(100);
  hnsw_param3.set_distance_type(vectordb::DISTANCE_TYPE_L2);

  EXPECT_EQ(vectordb::RET_OK, table.BuildIndex(hnsw_param3));

  // 使用第二个索引进行搜索
  std::vector<int64_t> ids2;
  std::vector<float> distances2;
  std::vector<std::string> scalars2;

  EXPECT_EQ(vectordb::RET_OK,
            table.Search(query_vector, 3, ids2, distances2, scalars2));

  // 验证搜索结果
  ASSERT_EQ(size_t(3), ids2.size());
  ASSERT_EQ(size_t(3), distances2.size());
  ASSERT_EQ(size_t(3), scalars2.size());

  // 比较两次搜索结果是否相同
  EXPECT_EQ(ids1, ids2);

  // 验证距离值是否相同（考虑浮点数精度问题，使用近似比较）
  for (size_t i = 0; i < distances1.size(); i++) {
    EXPECT_NEAR(distances1[i], distances2[i], 1e-6);
  }

  // 验证标量数据是否相同
  EXPECT_EQ(scalars1, scalars2);

  // 打印搜索结果
  std::cout << "搜索结果比较：" << std::endl;
  for (size_t i = 0; i < ids1.size(); i++) {
    std::cout << "索引1 - id: " << ids1[i] << ", distance: " << distances1[i]
              << std::endl;
    std::cout << "索引2 - id: " << ids2[i] << ", distance: " << distances2[i]
              << std::endl;
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}