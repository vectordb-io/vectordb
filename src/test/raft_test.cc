#include "raft.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

#include "coding.h"
#include "ping.h"
#include "ping_reply.h"
#include "raft.h"
#include "raft_server.h"
#include "remu.h"
#include "tracer.h"

void GenerateConfig(std::vector<vraft::Config> &configs, int32_t peers_num) {
  vraft::GetConfig().set_my_addr(vraft::HostPort("127.0.0.1", 9000));
  for (int i = 1; i <= peers_num; ++i) {
    vraft::GetConfig().peers().push_back(
        vraft::HostPort("127.0.0.1", 9000 + i));
  }
  vraft::GetConfig().set_log_level(vraft::kLoggerTrace);
  vraft::GetConfig().set_enable_debug(true);
  vraft::GetConfig().set_path("/tmp/remu_test_dir");
  vraft::GetConfig().set_mode(vraft::kSingleMode);

  vraft::GenerateRotateConfig(configs);
}

TEST(Remu, Print) {
  system("rm -rf /tmp/remu_print_test");
  vraft::LoggerOptions o;
  o.logger_name = "Remu.Print";
  vraft::vraft_logger.Init("/tmp/remu_print_test", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("remu");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::Remu remu(loop);
  GenerateConfig(remu.configs, 4);
  remu.Create();
  remu.Print();

  system("rm -rf /tmp/remu_print_test");
}

TEST(Raft, test) {
  system("rm -rf /tmp/raft_load_test");
  vraft::RaftConfig rc;
  rc.me = vraft::RaftAddr("127.0.0.1", 9000, 100);
  for (uint16_t i = 1; i <= 2; ++i) {
    uint16_t port = 9000 + i;
    rc.peers.push_back(vraft::RaftAddr("127.0.0.1", port, 100));
  }
  vraft::Raft r("/tmp/raft_load_test", rc);
  r.Init();
  EXPECT_EQ(r.Id(), 100);
  EXPECT_EQ(r.Peers().size(), static_cast<size_t>(2));
  std::cout << r.ToJsonString(false, false) << std::endl;
  system("rm -rf /tmp/raft_load_test");
}

TEST(Raft, test2) {
  system("rm -rf /tmp/raft_load_test");
  {
    vraft::RaftConfig rc;
    rc.me = vraft::RaftAddr("127.0.0.1", 9000, 100);
    for (uint16_t i = 1; i <= 2; ++i) {
      uint16_t port = 9000 + i;
      rc.peers.push_back(vraft::RaftAddr("127.0.0.1", port, 100));
    }
    vraft::Raft r("/tmp/raft_load_test", rc);
    r.Init();
    EXPECT_EQ(r.Id(), 100);
    EXPECT_EQ(r.Peers().size(), static_cast<size_t>(2));
    std::cout << r.ToJsonString(false, false) << std::endl;
  }

  {
    vraft::RaftConfig rc;
    rc.me = vraft::RaftAddr("1.2.3.4", 77, 88);
    vraft::Raft r("/tmp/raft_load_test", rc);
    r.Init();
    EXPECT_EQ(r.Id(), 100);
    EXPECT_EQ(r.Peers().size(), static_cast<size_t>(2));
    std::cout << r.ToJsonString(false, false) << std::endl;
  }

  system("rm -rf /tmp/raft_load_test");
  {
    vraft::RaftConfig rc;
    rc.me = vraft::RaftAddr("1.2.3.4", 77, 88);
    vraft::Raft r("/tmp/raft_load_test", rc);
    r.Init();
    EXPECT_EQ(r.Id(), 88);
    EXPECT_EQ(r.Peers().size(), static_cast<size_t>(0));
    std::cout << r.ToJsonString(false, false) << std::endl;
  }

  system("rm -rf /tmp/raft_load_test");
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}