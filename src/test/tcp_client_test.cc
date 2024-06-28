#include "tcp_client.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>
#include <thread>

#include "echo_server.h"
#include "eventloop.h"
#include "raft.h"
#include "tcp_server.h"
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

TEST(TcpClient, TcpClient) {
  system("rm -f /tmp/tcpclient_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "TcpClient.TcpClient";
  vraft::vraft_logger.Init("/tmp/tcpclient_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::HostPort dest_addr("baidu.com:80");
  vraft::TcpOptions to;
  vraft::TcpClient tcp_client(loop, "tcp_client", dest_addr, to);

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();

  vraft::TcpClient *pt = &tcp_client;
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

TEST(TcpClient, TcpClient2) {
  system("rm -f /tmp/tcpclient_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "TcpClient.TcpClient2";
  vraft::vraft_logger.Init("/tmp/tcpclient_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::HostPort dest_addr("baidu.com:80");
  vraft::TcpOptions to;
  vraft::TcpClient tcp_client(loop, "tcp_client", dest_addr, to);
  vraft::TcpClient *pt = &tcp_client;

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();

  loop->RunFunctor([pt]() { pt->Connect(); });
  std::this_thread::sleep_for(std::chrono::seconds(1));
  loop->RunFunctor([pt]() { std::cout << pt->DebugString() << std::endl; });

  std::this_thread::sleep_for(std::chrono::seconds(2));
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

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}