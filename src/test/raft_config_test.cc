#include "raft_config.h"

#include <gtest/gtest.h>

#include <iostream>

#include "simple_random.h"

TEST(RaftConfig, RaftConfig) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9002, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  {
    auto addr = vraft::RaftAddr("127.0.0.1", 9000, 7);
    bool b = c.InConfig(addr);
    ASSERT_EQ(b, true);
  }

  {
    auto addr = vraft::RaftAddr("127.0.0.1", 9001, 7);
    bool b = c.InConfig(addr);
    ASSERT_EQ(b, true);
  }

  {
    auto addr = vraft::RaftAddr("127.0.0.1", 9002, 7);
    bool b = c.InConfig(addr);
    ASSERT_EQ(b, true);
  }
}

TEST(RaftConfig, IamIn) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  std::cout << c.ToJsonString(false, false) << std::endl;

  vraft::RaftConfig c2;
  c2.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9002, 7));
  std::cout << c2.ToJsonString(false, false) << std::endl;

  bool b = c.IamIn(c2);
  ASSERT_EQ(b, true);
}

TEST(RaftConfig, IamIn2) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  vraft::RaftConfig c2;
  c2.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9002, 7));
  std::cout << c2.ToJsonString(false, false) << std::endl;

  bool b = c.IamIn(c2);
  ASSERT_EQ(b, true);
}

TEST(RaftConfig, IamIn3) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 888);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  vraft::RaftConfig c2;
  c2.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9002, 7));
  std::cout << c2.ToJsonString(false, false) << std::endl;

  bool b = c.IamIn(c2);
  ASSERT_EQ(b, false);
}

TEST(RaftConfig, IamIn4) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 888));
  std::cout << c.ToJsonString(false, false) << std::endl;

  vraft::RaftConfig c2;
  c2.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9002, 7));
  std::cout << c2.ToJsonString(false, false) << std::endl;

  bool b = c.IamIn(c2);
  ASSERT_EQ(b, false);
}

TEST(RaftConfig, IamIn5) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  vraft::RaftConfig c2;
  c2.me = vraft::RaftAddr("127.0.0.1", 9002, 7);
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9000, 7));
  std::cout << c2.ToJsonString(false, false) << std::endl;

  bool b = c.IamIn(c2);
  ASSERT_EQ(b, true);
}

TEST(RaftConfig, ToVector) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9002, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  std::vector<vraft::RaftAddr> v = c.ToVector();
  ASSERT_EQ(v.size(), (size_t)3);
  for (auto addr : v) {
    std::cout << addr.ToString() << std::endl;
  }
}

TEST(RaftConfig, ToSet) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9002, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  std::set<vraft::RaftAddr> s = c.ToSet();
  ASSERT_EQ(s.size(), (size_t)3);
  for (auto addr : s) {
    std::cout << addr.ToString() << std::endl;
  }
}

TEST(RaftConfig, ConfigCompare) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  vraft::RaftConfig c2;
  c2.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9002, 7));
  c2.peers.push_back(vraft::RaftAddr("127.0.0.1", 9003, 7));
  std::cout << c2.ToJsonString(false, false) << std::endl;

  {
    bool b = c.IamIn(c2);
    ASSERT_EQ(b, true);

    std::vector<vraft::RaftAddr> diff;
    int32_t rv = vraft::ConfigCompare(c, c2, diff);
    ASSERT_EQ(rv, -1);
    ASSERT_EQ(diff.size(), (size_t)2);
    for (auto addr : diff) {
      std::cout << addr.ToString() << " ";
    }
    std::cout << std::endl;
  }

  {
    bool b = c2.IamIn(c);
    ASSERT_EQ(b, false);

    std::vector<vraft::RaftAddr> diff;
    int32_t rv = vraft::ConfigCompare(c2, c, diff);
    ASSERT_EQ(rv, 1);
    ASSERT_EQ(diff.size(), (size_t)2);
    for (auto addr : diff) {
      std::cout << addr.ToString() << " ";
    }
    std::cout << std::endl;
  }
}

TEST(RaftConfig, ConfigCompare2) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  std::vector<vraft::RaftAddr> diff;
  int32_t rv = vraft::ConfigCompare(c, c, diff);
  ASSERT_EQ(rv, 0);
  ASSERT_EQ(diff.size(), (size_t)0);
  std::cout << std::endl;
}

TEST(RaftConfig, ConfigCompare3) {
  vraft::RaftConfig c;
  c.me = vraft::RaftAddr("127.0.0.1", 9000, 7);
  c.peers.push_back(vraft::RaftAddr("127.0.0.1", 9001, 7));
  std::cout << c.ToJsonString(false, false) << std::endl;

  vraft::RaftConfig c2;
  c2.me = vraft::RaftAddr("127.0.0.1", 9000, 888);
  std::cout << c.ToJsonString(false, false) << std::endl;

  std::vector<vraft::RaftAddr> diff;
  int32_t rv = vraft::ConfigCompare(c, c2, diff);
  ASSERT_EQ(rv, -2);
  ASSERT_EQ(diff.size(), (size_t)0);
  std::cout << std::endl;
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}