#include "raft_server.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "acceptor.h"
#include "eventloop.h"
#include "hostport.h"
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

void GenerateConfig(int32_t peers_num) {
  vraft::GetConfig().set_my_addr(vraft::HostPort("127.0.0.1", 9000));
  for (int i = 1; i <= peers_num; ++i) {
    vraft::GetConfig().peers().push_back(
        vraft::HostPort("127.0.0.1", 9000 + i));
  }
  vraft::GetConfig().set_log_level(vraft::kLoggerTrace);
  vraft::GetConfig().set_enable_debug(true);
  vraft::GetConfig().set_path("/tmp/raftserver_test_dir");
  vraft::GetConfig().set_mode(vraft::kSingleMode);
}

TEST(RaftServer, RaftServer) {
  system("rm -f /tmp/raftserver_test_dir/log/raft.log");
  vraft::LoggerOptions o;
  o.logger_name = "RaftServer.RaftServer";
  vraft::vraft_logger.Init("/tmp/raftserver_test_dir/log/raft.log", o);
  GenerateConfig(2);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::RaftServer server(loop, vraft::GetConfig());
  vraft::RaftServer *ptr = &server;
  loop->RunFunctor([ptr]() { ptr->Start(); });

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();

  loop->RunFunctor([ptr]() { ptr->Stop(); });
  std::thread t2([loop]() {
    std::cout << "after 3s, call loop stop() ..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    loop->Stop();
  });
  t.join();
  t2.join();
  std::cout << "loop stop" << std::endl;
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}