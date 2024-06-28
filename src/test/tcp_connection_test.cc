#include "tcp_connection.h"

#include <gtest/gtest.h>

#include <atomic>
#include <csignal>
#include <iostream>

#include "eventloop.h"
#include "raft.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"
#include "uv_wrapper.h"

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

vraft::EventLoopSPtr lptr;
vraft::UvTcpUPtr *uptr;
std::atomic<bool> b(false);

void ConnCb(vraft::UvConnect *req, int status) { b.store(true); }

void ConnectLoop() {
  vraft::UvConnect req;
  vraft::HostPort hp("baidu.com:80");
  int32_t rv = vraft::UvTcpConnect(&req, uptr->get(),
                                   (const struct sockaddr *)&hp.addr, ConnCb);
  assert(rv == 0);
  lptr->Loop();
}

TEST(TcpConnection, TcpConnection) {
  system("rm -f /tmp/tcp_connection_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "TcpConnection.TcpConnection";
  vraft::vraft_logger.Init("/tmp/tcp_connection_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::UvTcpUPtr ptr = std::make_unique<vraft::UvTcp>();
  uptr = &ptr;

  rv = vraft::UvTcpInit(loop->UvLoopPtr(), uptr->get());
  assert(rv == 0);

  lptr = loop;
  std::thread t(ConnectLoop);

  while (!b.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  vraft::TcpConnection conn(loop, "test_conn", std::move(ptr));
  std::cout << conn.DebugString() << std::endl;

  vraft::TcpConnection *pc = &conn;

  loop->RunFunctor([pc]() { pc->Close(); });
  std::thread t2([loop]() {
    std::cout << "after 3s, call loop stop() ..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    loop->Stop();
  });
  t.join();
  t2.join();
  lptr.reset();
  std::cout << "loop stop" << std::endl;
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}