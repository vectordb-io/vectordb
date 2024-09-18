#include "test_suite.h"

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

TEST(TestSM, TestSM) {
  std::string path = "/tmp/test_sm_test";
  std::string rm_cmd = "rm -rf " + path;
  system(rm_cmd.c_str());
  vraft::TestSM sm(path);
  std::cout << sm.ToJsonString(false, false) << std::endl;

  for (int32_t i = 0; i < 5; ++i) {
    vraft::LogEntry entry;
    entry.index = i;
    entry.append_entry.term = 100 + i;
    entry.append_entry.type = vraft::kData;
    char value_buf[128];
    entry.append_entry.value.clear();
    snprintf(value_buf, sizeof(value_buf), "key_%d:value_%d", i, i);
    entry.append_entry.value.append(value_buf);
    entry.pre_chk_all = 5;
    entry.CheckThis();
    entry.CheckAll();
    int32_t rv = sm.Apply(&entry, vraft::RaftAddr("127.0.0.1", 88, 9));
    ASSERT_EQ(rv, 0);
  }
  std::cout << sm.ToJsonString(false, false) << std::endl;

  for (int32_t i = 0; i < 5; ++i) {
    char key_buf[128];
    snprintf(key_buf, sizeof(key_buf), "key_%d", i);
    std::string value;
    int32_t rv = sm.Get(std::string(key_buf), value);
    ASSERT_EQ(rv, 1);

    char value_buf[128];
    snprintf(value_buf, sizeof(value_buf), "value_%d", i);
    ASSERT_EQ(value, std::string(value_buf));
    std::cout << "get: " << key_buf << " - " << value << std::endl;
  }
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}