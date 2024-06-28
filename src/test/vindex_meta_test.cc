#include "vindex_meta.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "raft.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"

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
TEST(VindexMeta, VindexMeta) {
  system("rm -rf /tmp/vindex_meta_test_dir");

  vectordb::VIndexParam param;
  param.path = "/tmp/vindex_meta_test_dir/index/xxxx";
  param.timestamp = vraft::Clock::NSec();
  param.dim = dim;
  param.index_type = vectordb::kIndexAnnoy;
  param.distance_type = vectordb::kCosine;
  param.annoy_tree_num = 10;

  {
    vectordb::VindexMeta meta("/tmp/vindex_meta_test_dir", param);
    std::cout << meta.ToJsonString(false, true) << std::endl;
  }

  vectordb::VIndexParam param_nouse;
  param_nouse.path = "/tmp/vindex_meta_test_dir/index/xxxxyyyyy";
  param_nouse.timestamp = vraft::Clock::NSec();
  param_nouse.dim = 329342;
  param_nouse.index_type = vectordb::kIndexAnnoy;
  param_nouse.distance_type = vectordb::kInnerProduct;
  param_nouse.annoy_tree_num = 99999;

  {
    vectordb::VindexMeta meta("/tmp/vindex_meta_test_dir", param_nouse);
    std::cout << meta.ToJsonString(false, true) << std::endl;

    vectordb::VIndexParam param2 = meta.param();

    ASSERT_EQ(param.path, param2.path);
    ASSERT_EQ(param.timestamp, param2.timestamp);
    ASSERT_EQ(param.dim, param2.dim);
    ASSERT_EQ(param.index_type, param2.index_type);
    ASSERT_EQ(param.distance_type, param2.distance_type);
    ASSERT_EQ(param.annoy_tree_num, param2.annoy_tree_num);
  }
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}