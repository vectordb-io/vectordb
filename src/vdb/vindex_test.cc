#include "vindex.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <experimental/filesystem>
#include <fstream>
#include <string>

#include "common.h"
#include "util.h"

const std::string kTestDir = "/tmp/vindex_test";

// 测试 VIndex 的 New 方法
TEST(VIndexTest, New) {
  // 确保测试目录不存在
  if (fs::exists(kTestDir)) {
    fs::remove_all(kTestDir);
  }

  // 创建 IndexParam 对象
  vdb::IndexParam param;
  param.set_path(kTestDir);
  param.set_id(1);
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.mutable_index_info()->set_index_type(vectordb::INDEX_TYPE_FLAT);
  param.mutable_index_info()->mutable_flat_param()->set_dim(128);
  param.mutable_index_info()->mutable_flat_param()->set_max_elements(1000);
  param.mutable_index_info()->mutable_flat_param()->set_distance_type(
      vectordb::DISTANCE_TYPE_L2);

  {
    // 创建 VIndex 对象，这将触发 New 方法
    vectordb::VIndex index(param);

    // 验证目录和文件是否被创建
    EXPECT_TRUE(fs::exists(kTestDir));
    EXPECT_TRUE(fs::exists(kTestDir + "/data"));
    EXPECT_TRUE(fs::exists(kTestDir + "/description.json"));

    // 验证描述文件内容
    std::ifstream file(kTestDir + "/description.json");
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // 检查描述文件内容是否包含索引类型和维度信息
    EXPECT_TRUE(content.find("\"dim\": 128") != std::string::npos);
    EXPECT_TRUE(content.find("\"max_elements\": 1000") != std::string::npos);
  }

  // 清理测试目录
  // fs::remove_all(kTestDir);
}

// 测试 VIndex 的 Add 方法, INDEX_TYPE_FLAT, DISTANCE_TYPE_L2
TEST(VIndexTest, Add) {
  // 确保测试目录不存在
  if (fs::exists(kTestDir)) {
    fs::remove_all(kTestDir);
  }

  // 创建 IndexParam 对象
  vdb::IndexParam param;
  param.set_path(kTestDir);
  param.set_id(1);
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.mutable_index_info()->set_index_type(vectordb::INDEX_TYPE_FLAT);
  param.mutable_index_info()->mutable_flat_param()->set_dim(3);
  param.mutable_index_info()->mutable_flat_param()->set_max_elements(100);
  param.mutable_index_info()->mutable_flat_param()->set_distance_type(
      vectordb::DISTANCE_TYPE_L2);

  // 创建 VIndex 对象
  vectordb::VIndex index(param);

  // 添加向量
  std::vector<float> vec1 = {1.0, 2.0, 3.0};
  std::vector<float> vec2 = {4.0, 5.0, 6.0};
  std::vector<float> vec3 = {7.0, 8.0, 9.0};

  EXPECT_EQ(vectordb::RET_OK, index.Add(1, vec1));
  EXPECT_EQ(vectordb::RET_OK, index.Add(2, vec2));
  EXPECT_EQ(vectordb::RET_OK, index.Add(3, vec3));

  EXPECT_EQ(index.Size(), 3);

  // 清理测试目录
  fs::remove_all(kTestDir);
}

// 测试 VIndex 的 Search 方法, INDEX_TYPE_FLAT, DISTANCE_TYPE_L2
TEST(VIndexTest, Search) {
  // 确保测试目录不存在
  if (fs::exists(kTestDir)) {
    fs::remove_all(kTestDir);
  }

  // 创建 IndexParam 对象
  vdb::IndexParam param;
  param.set_path(kTestDir);
  param.set_id(1);
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.mutable_index_info()->set_index_type(vectordb::INDEX_TYPE_FLAT);
  param.mutable_index_info()->mutable_flat_param()->set_dim(3);
  param.mutable_index_info()->mutable_flat_param()->set_max_elements(100);
  param.mutable_index_info()->mutable_flat_param()->set_distance_type(
      vectordb::DISTANCE_TYPE_L2);

  // 创建 VIndex 对象
  vectordb::VIndex index(param);

  // 添加向量
  std::vector<float> vec1 = {1.0, 2.0, 3.0};
  std::vector<float> vec2 = {4.0, 5.0, 6.0};
  std::vector<float> vec3 = {7.0, 8.0, 9.0};
  std::vector<float> vec4 = {1.1, 2.1, 3.1};  // 接近 vec1

  EXPECT_EQ(vectordb::RET_OK, index.Add(1, vec1));
  EXPECT_EQ(vectordb::RET_OK, index.Add(2, vec2));
  EXPECT_EQ(vectordb::RET_OK, index.Add(3, vec3));
  EXPECT_EQ(vectordb::RET_OK, index.Add(4, vec4));

  // 测试向量搜索
  {
    std::vector<int64_t> ids;
    std::vector<float> distances;

    // 使用 vec1 搜索最近的 2 个向量
    EXPECT_EQ(vectordb::RET_OK, index.Search(vec1, 2, ids, distances));
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_EQ(distances.size(), 2u);

    // 打印结果，帮助调试
    std::cout << "Search results for vec1:" << std::endl;
    for (size_t i = 0; i < ids.size(); i++) {
      std::cout << "id: " << ids[i] << ", distance: " << distances[i]
                << std::endl;
    }

    // 由于 L2 距离，vec1 应该最接近自己，然后是 vec4
    // 检查结果中包含正确的ID，但不假设特定顺序
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), 1) != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), 4) != ids.end());
  }

  // 测试id搜索
  {
    std::vector<int64_t> ids;
    std::vector<float> distances;

    EXPECT_EQ(vectordb::RET_OK, index.Search(1, 2, ids, distances));
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_EQ(distances.size(), 2u);

    EXPECT_TRUE(std::find(ids.begin(), ids.end(), 1) != ids.end());
    EXPECT_TRUE(std::find(ids.begin(), ids.end(), 4) != ids.end());
  }

  // 清理测试目录
  fs::remove_all(kTestDir);
}

// 测试k的数量大于索引中的元素数量
TEST(VIndexTest, SearchKGreaterThanSize) {
  // 确保测试目录不存在
  if (fs::exists(kTestDir)) {
    fs::remove_all(kTestDir);
  }

  // 创建 IndexParam 对象
  vdb::IndexParam param;
  param.set_path(kTestDir);
  param.set_id(1);
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.mutable_index_info()->set_index_type(vectordb::INDEX_TYPE_FLAT);
  param.mutable_index_info()->mutable_flat_param()->set_dim(3);
  param.mutable_index_info()->mutable_flat_param()->set_max_elements(100);
  param.mutable_index_info()->mutable_flat_param()->set_distance_type(
      vectordb::DISTANCE_TYPE_L2);

  // 创建 VIndex 对象
  vectordb::VIndex index(param);

  // 添加向量
  std::vector<float> vec1 = {1.0, 2.0, 3.0};
  std::vector<float> vec2 = {4.0, 5.0, 6.0};
  std::vector<float> vec3 = {7.0, 8.0, 9.0};

  EXPECT_EQ(vectordb::RET_OK, index.Add(1, vec1));
  EXPECT_EQ(vectordb::RET_OK, index.Add(2, vec2));
  EXPECT_EQ(vectordb::RET_OK, index.Add(3, vec3));

  // 验证索引大小为3
  EXPECT_EQ(index.Size(), 3);

  // 测试向量搜索，k值大于索引中的元素数量
  std::vector<int64_t> ids;
  std::vector<float> distances;

  // 使用 vec1 搜索最近的 5 个向量（实际只有3个向量）
  EXPECT_EQ(vectordb::RET_OK, index.Search(vec1, 5, ids, distances));

  // 验证返回的结果数量等于实际的索引大小
  EXPECT_EQ(ids.size(), 3u);
  EXPECT_EQ(distances.size(), 3u);

  // 打印结果，帮助调试
  std::cout << "Search results for vec1 with k=5:" << std::endl;
  for (size_t i = 0; i < ids.size(); i++) {
    std::cout << "id: " << ids[i] << ", distance: " << distances[i]
              << std::endl;
  }

  // 验证所有ID都被返回
  EXPECT_TRUE(std::find(ids.begin(), ids.end(), 1) != ids.end());
  EXPECT_TRUE(std::find(ids.begin(), ids.end(), 2) != ids.end());
  EXPECT_TRUE(std::find(ids.begin(), ids.end(), 3) != ids.end());

  // 清理测试目录
  fs::remove_all(kTestDir);
}

void TEST_AddHNSWInnerProduct() {
  // 确保测试目录不存在
  if (fs::exists(kTestDir)) {
    fs::remove_all(kTestDir);
  }

  // 创建 IndexParam 对象
  vdb::IndexParam param;
  param.set_path(kTestDir);
  param.set_id(1);
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.mutable_index_info()->set_index_type(vectordb::INDEX_TYPE_HNSW);
  param.mutable_index_info()->mutable_hnsw_param()->set_dim(3);
  param.mutable_index_info()->mutable_hnsw_param()->set_max_elements(100);
  param.mutable_index_info()->mutable_hnsw_param()->set_distance_type(
      vectordb::DISTANCE_TYPE_INNER_PRODUCT);
  param.mutable_index_info()->mutable_hnsw_param()->set_ef_construction(200);
  param.mutable_index_info()->mutable_hnsw_param()->set_m(16);

  // 创建 VIndex 对象
  vectordb::VIndex index(param);

  // 添加向量
  std::vector<float> vec1 = {1.0, 2.0, 3.0};
  std::vector<float> vec2 = {4.0, 5.0, 6.0};
  std::vector<float> vec3 = {2.0, 4.0, 6.0};

  EXPECT_EQ(vectordb::RET_OK, index.Add(1, vec1));
  EXPECT_EQ(vectordb::RET_OK, index.Add(2, vec2));
  EXPECT_EQ(vectordb::RET_OK, index.Add(3, vec3));

  EXPECT_EQ(index.Size(), 3);

  // 测试 GetVecByID
  {
    std::vector<float> vec;
    EXPECT_EQ(vectordb::RET_OK, index.GetVecByID(1, vec));
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 1.0);
    EXPECT_EQ(vec[1], 2.0);
    EXPECT_EQ(vec[2], 3.0);
  }

  // 持久化索引
  EXPECT_EQ(vectordb::RET_OK, index.Persist());
}

// 测试 VIndex 的 Add 方法, INDEX_TYPE_HNSW, DISTANCE_TYPE_INNER_PRODUCT
TEST(VIndexTest, AddHNSWInnerProduct) {
  TEST_AddHNSWInnerProduct();

  // 清理测试目录
  // fs::remove_all(kTestDir);
}

// 测试 VIndex 的 Load 方法, INDEX_TYPE_HNSW, DISTANCE_TYPE_INNER_PRODUCT
TEST(VIndexTest, LoadHNSWInnerProduct) {
  { TEST_AddHNSWInnerProduct(); }

  // 创建 IndexParam 对象
  vdb::IndexParam param;
  param.set_path(kTestDir);
  param.set_id(1);
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.mutable_index_info()->set_index_type(vectordb::INDEX_TYPE_HNSW);
  param.mutable_index_info()->mutable_hnsw_param()->set_dim(3);
  param.mutable_index_info()->mutable_hnsw_param()->set_max_elements(100);
  param.mutable_index_info()->mutable_hnsw_param()->set_distance_type(
      vectordb::DISTANCE_TYPE_INNER_PRODUCT);
  param.mutable_index_info()->mutable_hnsw_param()->set_ef_construction(200);
  param.mutable_index_info()->mutable_hnsw_param()->set_m(16);

  // 创建 VIndex 对象
  vectordb::VIndex index(param);

  EXPECT_EQ(index.Size(), 3);

  // 清理测试目录
  // fs::remove_all(kTestDir);
}

// 测试 VIndex 的 Search 方法, INDEX_TYPE_HNSW, DISTANCE_TYPE_INNER_PRODUCT
TEST(VIndexTest, SearchHNSWInnerProduct) {
  { TEST_AddHNSWInnerProduct(); }

  // 创建 IndexParam 对象
  vdb::IndexParam param;
  param.set_path(kTestDir);
  param.set_id(1);
  param.set_create_time(vectordb::TimeStamp().MilliSeconds());
  param.mutable_index_info()->set_index_type(vectordb::INDEX_TYPE_HNSW);
  param.mutable_index_info()->mutable_hnsw_param()->set_dim(3);
  param.mutable_index_info()->mutable_hnsw_param()->set_max_elements(100);
  param.mutable_index_info()->mutable_hnsw_param()->set_distance_type(
      vectordb::DISTANCE_TYPE_INNER_PRODUCT);
  param.mutable_index_info()->mutable_hnsw_param()->set_ef_construction(200);
  param.mutable_index_info()->mutable_hnsw_param()->set_m(16);

  // 创建 VIndex 对象
  vectordb::VIndex index(param);

  EXPECT_EQ(index.Size(), 3);

  // 测试search by id
  {
    std::vector<int64_t> ids;
    std::vector<float> distances;
    EXPECT_EQ(vectordb::RET_OK, index.Search(1, 2, ids, distances));
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 1);

    // 打印搜索结果
    std::cout << "Search results for id 1:" << std::endl;
    for (size_t i = 0; i < ids.size(); i++) {
      std::cout << "id: " << ids[i] << ", distance: " << distances[i]
                << std::endl;
    }
  }

  // 测试search by vector
  {
    std::vector<int64_t> ids;
    std::vector<float> distances;
    std::vector<float> vec1 = {1.0, 2.0, 3.0};
    EXPECT_EQ(vectordb::RET_OK, index.Search(vec1, 2, ids, distances));
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], 1);

    // 打印搜索结果
    std::cout << "Search results for vec1:" << std::endl;
    for (size_t i = 0; i < ids.size(); i++) {
      std::cout << "id: " << ids[i] << ", distance: " << distances[i]
                << std::endl;
    }
  }

  // 清理测试目录
  // fs::remove_all(kTestDir);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}