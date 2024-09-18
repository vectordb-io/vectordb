#include "config_manager.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "util.h"

TEST(RaftConfig, RaftConfig) {
  vraft::RaftConfig msg;
  msg.me.FromString("127.0.0.1:9000#7");

  vraft::RaftAddr addr;
  addr.FromString("127.0.0.1:9001#7");
  msg.peers.push_back(addr);
  addr.FromString("127.0.0.1:9002#7");
  msg.peers.push_back(addr);

  std::string msg_str;
  int32_t bytes = msg.ToString(msg_str);
  std::cout << "bytes:" << bytes << std::endl;

  std::cout << "encoding:" << std::endl;
  std::cout << msg.ToJsonString(true, true) << std::endl;
  std::cout << msg.ToJsonString(true, false) << std::endl;
  std::cout << msg.ToJsonString(false, true) << std::endl;
  std::cout << msg.ToJsonString(false, false) << std::endl;

  vraft::RaftConfig msg2;
  bytes = msg2.FromString(msg_str);
  assert(bytes > 0);

  std::cout << "decoding:" << std::endl;
  std::cout << msg2.ToJsonString(true, true) << std::endl;
  std::cout << msg2.ToJsonString(true, false) << std::endl;
  std::cout << msg2.ToJsonString(false, true) << std::endl;
  std::cout << msg2.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(msg.me.ToU64(), msg2.me.ToU64());
  ASSERT_EQ(msg.peers.size(), msg2.peers.size());
  for (size_t i = 0; i < msg.peers.size(); ++i) {
    ASSERT_EQ(msg.peers[i].ToU64(), msg2.peers[i].ToU64());
  }
}

TEST(ConfigManager, ConfigManager) {
  vraft::RaftConfig rc;
  rc.me.FromString("127.0.0.1:9000#7");

  vraft::RaftAddr addr;
  addr.FromString("127.0.0.1:9001#7");
  rc.peers.push_back(addr);

  vraft::ConfigManager mgr(rc);
  std::cout << mgr.ToJsonString(false, false);

  addr.FromString("127.0.0.1:9002#7");
  rc.peers.push_back(addr);

  mgr.SetCurrent(rc);
  std::cout << mgr.ToJsonString(false, false);

  mgr.Rollback();
  std::cout << mgr.ToJsonString(false, false);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}