#include "keyid_meta.h"

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

TEST(KeyidMeta, KeyidMeta) {
  system("rm -rf /tmp/keyid_meta_test_dir");
  vectordb::KeyidMeta meta("/tmp/keyid_meta_test_dir");
}

TEST(KeyidMeta, OP) {
  system("rm -rf /tmp/keyid_meta_test_dir");
  vectordb::KeyidMeta meta("/tmp/keyid_meta_test_dir");

  std::string key = "kkk";
  int32_t id = 5;
  meta.Put(key, id);

  int32_t id2;
  int32_t rv = meta.Get(key, id2);
  std::cout << id2 << std::endl;
  ASSERT_EQ(rv, 0);
  ASSERT_EQ(id, id2);
}

TEST(KeyidMeta, OP2) {
  system("rm -rf /tmp/keyid_meta_test_dir");
  vectordb::KeyidMeta meta("/tmp/keyid_meta_test_dir");

  std::string key = "kkk";
  int32_t id = 5;
  meta.Put(id, key);

  std::string key2;
  int32_t rv = meta.Get(id, key2);
  std::cout << key2 << std::endl;
  ASSERT_EQ(rv, 0);
  ASSERT_EQ(key, key2);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}