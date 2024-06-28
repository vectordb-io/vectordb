#include "tcp_server.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>
#include <thread>

#include "echo_server.h"
#include "eventloop.h"
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

TEST(TcpServer, TcpServer) {
  system("rm -f /tmp/tcpserver_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "TcpServer.TcpServer";
  vraft::vraft_logger.Init("/tmp/tcpserver_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::HostPort addr("127.0.0.1:9988");
  vraft::TcpOptions to;
  vraft::TcpServer tcp_server(loop, "test-tcp-server", addr, to);

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();

  vraft::TcpServer *pt = &tcp_server;
  loop->RunFunctor([pt]() { pt->Stop(); });

  std::thread t2([loop]() {
    std::cout << "after 3s, call loop stop() ..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    loop->Stop();
  });
  t.join();
  t2.join();
  std::cout << "loop stop" << std::endl;
}

TEST(TcpServer, EchoServer) {}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}