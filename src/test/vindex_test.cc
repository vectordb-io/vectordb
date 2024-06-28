#include "vindex.h"

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
TEST(VIndexParam, VIndexParam) {
  vectordb::VIndexParam param;
  param.path = "/tmp/vindex_annoy_test_dir/index/annoy";
  param.timestamp = vraft::Clock::NSec();
  param.dim = dim;
  param.index_type = vectordb::kIndexAnnoy;
  param.distance_type = vectordb::kCosine;
  param.annoy_tree_num = 10;

  std::string str;
  int32_t bytes = param.ToString(str);
  std::cout << "encoding bytes:" << bytes << std::endl;
  std::cout << param.ToJsonString(false, true) << std::endl;

  vectordb::VIndexParam param2;
  int32_t bytes2 = param2.FromString(str);
  assert(bytes2 > 0);

  std::cout << "decoding bytes:" << bytes2 << std::endl;
  std::cout << param2.ToJsonString(false, true) << std::endl;

  ASSERT_EQ(param.path, param2.path);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}