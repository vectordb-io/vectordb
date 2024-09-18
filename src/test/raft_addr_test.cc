#include "raft_addr.h"

#include <gtest/gtest.h>

#include <iostream>

#include "simple_random.h"

TEST(RaftAddr, RaftAddr) {
  vraft::RaftAddr addr;
  bool b = addr.FromString("127.0.0.1:9988#7");
  ASSERT_EQ(b, true);
  std::cout << addr.ToString() << std::endl;

  ASSERT_EQ(addr.port(), 9988);
  ASSERT_EQ(addr.id(), 7);
  ASSERT_EQ(addr.ToString(), std::string("127.0.0.1:9988#7"));
}

TEST(RaftAddr, Equal) {
  vraft::RaftAddr addr;
  bool b = addr.FromString("127.0.0.1:9988#7");
  ASSERT_EQ(b, true);
  std::cout << addr.ToString() << std::endl;

  vraft::RaftAddr addr2;
  b = addr2.FromString("127.0.0.1:9988#7");
  ASSERT_EQ(b, true);
  std::cout << addr.ToString() << std::endl;

  vraft::RaftAddr addr3;
  b = addr3.FromString("127.0.0.1:9988#8");
  ASSERT_EQ(b, true);
  std::cout << addr.ToString() << std::endl;

  ASSERT_EQ((addr == addr2), true);
  ASSERT_EQ((addr == addr3), false);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}