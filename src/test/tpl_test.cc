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

TEST(TPL, tpl) {
  EXPECT_TRUE(true);
  EXPECT_FALSE(false);
  EXPECT_EQ(1, 1);
  EXPECT_NE(1, 2);
  EXPECT_NE(1, 2);
  EXPECT_LE(2, 2);
  EXPECT_GT(3, 1);
  EXPECT_GE(3, 3);

  ASSERT_TRUE(true);
  ASSERT_FALSE(false);
  ASSERT_EQ(1, 1);
  ASSERT_NE(1, 2);
  ASSERT_NE(1, 2);
  ASSERT_LE(2, 2);
  ASSERT_GT(3, 1);
  ASSERT_GE(3, 3);
}

class MyTestClass : public ::testing::Test {
 protected:
  void SetUp() override { std::cout << "setting up test...\n"; }

  void TearDown() override { std::cout << "tearing down test...\n"; }
};

TEST_F(MyTestClass, test) {
  ASSERT_EQ(2 + 2, 4);
  std::cout << "exec MyTestClass.test ..." << std::endl;
}

TEST_F(MyTestClass, test2) {
  ASSERT_TRUE(true);
  std::cout << "exec MyTestClass.test2 ..." << std::endl;
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}