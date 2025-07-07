#include "retno.h"

#include <gtest/gtest.h>

TEST(RetNoTest, RetNoToString) {
  EXPECT_EQ(vectordb::RetNoToString(vectordb::RET_OK), "ok");
  EXPECT_EQ(vectordb::RetNoToString(vectordb::RET_ERROR), "error");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}