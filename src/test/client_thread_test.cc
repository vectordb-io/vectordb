#include "client_thread.h"

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

TEST(ClientThread, test) {
  system("rm -f /tmp/client_thread_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "ClientThread.ClientThread";
  vraft::vraft_logger.Init("/tmp/client_thread_test.log", o);

  vraft::ClientThread ct("test-client-thread", false);
  vraft::EventLoopSPtr loop = ct.LoopPtr();

  vraft::HostPort dest_addr("baidu.com:80");
  vraft::TcpOptions to;
  vraft::TcpClientSPtr tcp_client =
      std::make_shared<vraft::TcpClient>(loop, "tcp-client", dest_addr, to);
  tcp_client->set_close_cb(
      std::bind(&vraft::ClientThread::ServerCloseCountDown, &ct));

  ct.AddClient(tcp_client);
  ct.Start();
  vraft::ClientThread *pct = &ct;
  std::thread t([pct] {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    pct->Stop();
  });
  t.join();
  ct.Join();
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}