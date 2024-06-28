#include "timer.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>
#include <thread>

#include "eventloop.h"
#include "raft.h"
#include "test_suite.h"
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

void TimerCb(vraft::Timer *timer) { std::cout << "TimerCb" << std::endl; }

TEST(Timer, Timer) {
  system("rm -f /tmp/timer_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "Timer.Timer";
  vraft::vraft_logger.Init("/tmp/timer_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::TimerParam param;
  param.timeout_ms = 10;
  param.repeat_ms = 10;
  param.cb = TimerCb;
  param.data = nullptr;
  param.name = "test-timer";
  vraft::Timer timer(param, loop);
  vraft::Timer *pt = &timer;

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();
  loop->RunFunctor([pt]() { pt->Close(); });
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