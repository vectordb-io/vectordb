#include "vindex_annoy.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "raft.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"
#include "vengine.h"

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
TEST(VindexAnnoy, VindexAnnoy) {
  system("rm -rf /tmp/vindex_annoy_test_dir");

  bool ok = false;
  if (vraft::IsFileExist("./output/test/generate_vec_test")) {
    system("./output/test/generate_vec_test 10 100 > /tmp/vec.txt");
    ok = true;

  } else if (vraft::IsFileExist("./generate_vec_test")) {
    system("./generate_vec_test 10 100 > /tmp/vec.txt");
    ok = true;

  } else {
    std::cout << "\ngenerate_vec_test not exist !!!\n" << std::endl;
  }

  if (ok) {
    {
      vectordb::VEngineSPtr ve = std::make_shared<vectordb::VEngine>(
          "/tmp/vindex_annoy_test_dir", dim);
      std::cout << ve->ToJsonString(true, true) << std::endl;
      ASSERT_EQ(ve->Dim(), dim);

      int32_t rv = ve->Load("/tmp/vec.txt");
      ASSERT_EQ(rv, 0);

      vectordb::VIndexParam param;
      param.path = "/tmp/vindex_annoy_test_dir/index/annoy_test";
      param.timestamp = vraft::Clock::NSec();
      param.dim = dim;
      param.index_type = vectordb::kIndexAnnoy;
      param.distance_type = vectordb::kCosine;
      param.annoy_tree_num = 10;
      vectordb::VindexSPtr vindex =
          std::make_shared<vectordb::VindexAnnoy>(param, ve);
      assert(vindex);
      std::cout << vindex->ToJsonString(true, true) << std::endl;

      {
        std::string find_key = "key_3";
        std::vector<vectordb::VecResult> results;
        rv = vindex->GetKNN(find_key, results, 5);
        ASSERT_EQ(rv, 0);

        std::cout << "results.size(): " << results.size() << std::endl;
        for (auto &item : results) {
          std::cout << item.ToJsonString(true, true) << std::endl;
        }
      }

      {
        std::string find_key = "key_30";
        std::vector<vectordb::VecResult> results;
        rv = vindex->GetKNN(find_key, results, 20);
        ASSERT_EQ(rv, 0);

        std::cout << "results.size(): " << results.size() << std::endl;
        for (auto &item : results) {
          std::cout << item.ToJsonString(true, true) << std::endl;
        }
      }

      {
        std::string find_key = "key_40";
        std::vector<vectordb::VecResult> results;
        rv = vindex->GetKNN(find_key, results);
        ASSERT_EQ(rv, 0);

        std::cout << "results.size(): " << results.size() << std::endl;
        for (auto &item : results) {
          std::cout << item.ToJsonString(true, true) << std::endl;
        }
      }
    }
  }
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}