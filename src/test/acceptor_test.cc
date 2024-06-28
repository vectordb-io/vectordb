#include "acceptor.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

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

TEST(Acceptor, Acceptor) {
  system("rm -f /tmp/acceptor_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "Acceptor.Acceptor";
  vraft::vraft_logger.Init("/tmp/acceptor_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::HostPort addr("127.0.0.1:9988");
  vraft::TcpOptions to;
  vraft::Acceptor acceptor(loop, addr, to);
  vraft::Acceptor *a = &acceptor;

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();

  loop->RunFunctor([a]() { a->Close(); });
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