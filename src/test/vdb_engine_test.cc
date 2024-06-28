#include "vdb_engine.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "raft.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"
#include "vraft_logger.h"

//--------------------------------
// EXPECT_TRUE  true
// EXPECT_FALSE false
//
// ASSERT_TRUE  true
// ASSERT_FALSE false
//
// EXPECT_EQ  ==
// EXPECT_NE  !=
// EXPECT_NE  <
// EXPECT_LE  <=
// EXPECT_GT  >
// EXPECT_GE  >=
//
// ASSERT_EQ  ==
// ASSERT_NE  !=
// ASSERT_LT  <
// ASSERT_LE  <=
// ASSERT_GT  >
// ASSERT_GE  >=
//--------------------------------

int32_t dim = 10;
std::string home_path = "/tmp/vdb_engine_test_dir";

TEST(VdbEngine, VdbEngine) {
  system((std::string("rm -rf ") + home_path).c_str());
  std::string key = "kkk";

  {
    vectordb::VdbEngine vdb(home_path);
    std::cout << vdb.ToJsonString(false, false) << std::endl;

    vectordb::AddTableParam param;
    param.name = "test-table";
    param.partition_num = 10;
    param.replica_num = 3;
    param.dim = dim;
    vdb.AddTable(param);
    std::cout << vdb.ToJsonString(false, false) << std::endl;

    vectordb::VecValue vv;
    for (int32_t i = 0; i < dim; ++i) {
      float f32 = vraft::RandomFloat(1);
      vv.vec.data.push_back(f32);
    }
    vv.attach_value = "aaavvv";
    int32_t rv = vdb.Put(param.name, key, vv);
    ASSERT_EQ(rv, 0);
    std::cout << "put: " << vv.ToJsonString(false, true) << std::endl;

    vectordb::VecObj vo;
    rv = vdb.Get(param.name, key, vo);
    ASSERT_EQ(rv, 0);
    std::cout << "get: " << vo.ToJsonString(false, true) << std::endl;

    ASSERT_EQ(vo.key, key);
    ASSERT_EQ(vo.vv.vec.data.size(), vv.vec.data.size());
    for (size_t i = 0; i < vo.vv.vec.data.size(); ++i) {
      ASSERT_EQ(vo.vv.vec.data[i], vv.vec.data[i]);
    }
    ASSERT_EQ(vo.vv.attach_value, vv.attach_value);
  }
}

TEST(VdbEngine, Load) {
  system((std::string("rm -rf ") + home_path).c_str());

  bool ok = false;
  if (vraft::IsFileExist("./output/test/generate_vec_test")) {
    system("./output/test/generate_vec_test 10 1000 > /tmp/vec.txt");
    ok = true;

  } else if (vraft::IsFileExist("./generate_vec_test")) {
    system("./generate_vec_test 10 100 > /tmp/vec.txt");
    ok = true;

  } else {
    std::cout << "\ngenerate_vec_test not exist !!!\n" << std::endl;
  }

  if (ok) {
    vectordb::VdbEngine vdb(home_path);
    std::cout << vdb.ToJsonString(false, false) << std::endl;

    vectordb::AddTableParam param;
    param.name = "test-table";
    param.partition_num = 10;
    param.replica_num = 3;
    param.dim = dim;
    vdb.AddTable(param);
    std::cout << vdb.ToJsonString(false, false) << std::endl;

    int32_t rv = vdb.Load(param.name, "/tmp/vec.txt");
    ASSERT_EQ(rv, 0);

    vectordb::VecObj vo;
    rv = vdb.Get(param.name, "key_0", vo);
    ASSERT_EQ(rv, 0);
    std::cout << "get: " << vo.ToJsonString(false, true) << std::endl;

    vectordb::AddIndexParam add_index_param;
    add_index_param.timestamp = vraft::Clock::NSec();
    add_index_param.dim = dim;
    add_index_param.index_type = vectordb::kIndexAnnoy;
    add_index_param.distance_type = vectordb::kCosine;
    add_index_param.annoy_tree_num = 20;
    rv = vdb.AddIndex(param.name, add_index_param);
    ASSERT_EQ(rv, 0);

    add_index_param.timestamp = vraft::Clock::NSec();
    rv = vdb.AddIndex(param.name, add_index_param);
    ASSERT_EQ(rv, 0);

    std::cout << vdb.ToJsonString(false, false) << std::endl;

    {
      std::string find_key = "key_3";
      std::vector<vectordb::VecResult> results;
      rv = vdb.GetKNN(param.name, find_key, results, 10);
      ASSERT_EQ(rv, 0);
      ASSERT_EQ(results.size(), (size_t)10);

      std::cout << "results.size(): " << results.size() << std::endl;
      for (auto &item : results) {
        std::cout << item.ToJsonString(true, true) << std::endl;
      }
    }

    {
      std::vector<float> vec;
      for (int32_t i = 0; i < dim; ++i) {
        float f32 = vraft::RandomFloat(1);
        vec.push_back(f32);
      }

      std::vector<vectordb::VecResult> results;
      rv = vdb.GetKNN(param.name, vec, results, 10);
      ASSERT_EQ(rv, 0);
      ASSERT_EQ(results.size(), (size_t)10);

      std::cout << "results.size(): " << results.size() << std::endl;
      for (auto &item : results) {
        std::cout << item.ToJsonString(true, true) << std::endl;
      }
    }
  }

  if (ok) {
    vectordb::VdbEngine vdb(home_path);
    std::cout << vdb.ToJsonString(false, false) << std::endl;

    std::string table_name = "test-table";
    int32_t rv = 0;

    vectordb::VecObj vo;
    rv = vdb.Get(table_name, "key_0", vo);
    ASSERT_EQ(rv, 0);
    std::cout << "get: " << vo.ToJsonString(false, true) << std::endl;

    {
      std::string find_key = "key_3";
      std::vector<vectordb::VecResult> results;
      rv = vdb.GetKNN(table_name, find_key, results, 10);
      ASSERT_EQ(rv, 0);
      ASSERT_EQ(results.size(), (size_t)10);

      std::cout << "results.size(): " << results.size() << std::endl;
      for (auto &item : results) {
        std::cout << item.ToJsonString(true, true) << std::endl;
      }
    }

    {
      std::vector<float> vec;
      for (int32_t i = 0; i < dim; ++i) {
        float f32 = vraft::RandomFloat(1);
        vec.push_back(f32);
      }

      std::vector<vectordb::VecResult> results;
      rv = vdb.GetKNN(table_name, vec, results, 10);
      ASSERT_EQ(rv, 0);
      ASSERT_EQ(results.size(), (size_t)10);

      std::cout << "results.size(): " << results.size() << std::endl;
      for (auto &item : results) {
        std::cout << item.ToJsonString(true, true) << std::endl;
      }
    }
  }
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  vraft::LoggerOptions logger_options{
      "vraft", false, 1, 8192, vraft::kLoggerTrace, true};
  std::string log_file = home_path + "/log/test.log";
  vraft::vraft_logger.Init(log_file, logger_options);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}