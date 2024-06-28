#include "engine_meta.h"

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

TEST(EngineMeta, EngineMeta) {
  system("rm -rf /tmp/engine_meta_test_dir");
  vectordb::EngineMeta meta("/tmp/engine_meta_test_dir");
  std::cout << "dim: " << meta.dim() << std::endl;
  ASSERT_EQ(meta.dim(), 0);
}

TEST(EngineMeta, PersistDim) {
  system("rm -rf /tmp/engine_meta_test_dir");

  {
    vectordb::EngineMeta meta("/tmp/engine_meta_test_dir");
    std::cout << meta.ToJsonString(false, true) << std::endl;
    ASSERT_EQ(meta.dim(), 0);

    meta.SetDim(3);
    std::cout << meta.ToJsonString(false, true) << std::endl;
    ASSERT_EQ(meta.dim(), 3);
  }

  {
    vectordb::EngineMeta meta("/tmp/engine_meta_test_dir");
    std::cout << meta.ToJsonString(false, true) << std::endl;
    ASSERT_EQ(meta.dim(), 3);
  }
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}