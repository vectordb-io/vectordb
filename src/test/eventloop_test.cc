#include "eventloop.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>
#include <thread>

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

// if loop don't run, resource will not be freed,
// so cannot pass the valgrind check
#if 0
TEST(EventLoop, EventLoop) {
  system("rm -f /tmp/eventloop_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "EventLoop.EventLoop";
  vraft::vraft_logger.Init("/tmp/eventloop_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  // loop->Stop();
}
#endif

TEST(EventLoop, EventLoop2) {
  system("rm -f /tmp/eventloop_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "EventLoop.EventLoop2";
  vraft::vraft_logger.Init("/tmp/eventloop_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();
  std::thread t2([loop]() {
    std::cout << "after 3s, call loop stop() ..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    loop->Stop();
  });
  t.join();
  t2.join();
  std::cout << "loop stop" << std::endl;
}

void TimerCb(vraft::Timer *timer) { std::cout << "TimerCb" << std::endl; }

TEST(EventLoop, AddTimer) {
  system("rm -f /tmp/eventloop_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "EventLoop.AddTimer";
  vraft::vraft_logger.Init("/tmp/eventloop_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::TimerParam param;
  param.timeout_ms = 1000;
  param.repeat_ms = 1000;
  param.cb = TimerCb;
  param.data = nullptr;
  param.name = "test-timer";
  loop->AddTimer(param);

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();
  std::thread t2([loop]() {
    std::cout << "after 5s, call loop stop() ..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    loop->Stop();
  });
  t.join();
  t2.join();
  std::cout << "loop stop" << std::endl;
}

void TimerCb2(vraft::Timer *timer) {
  std::cout << "TimerCb2 repeat_counter:" << timer->repeat_counter()
            << std::endl;
  timer->RepeatDecr();
  if (timer->repeat_counter() == 0) {
    auto sptr = timer->LoopSPtr();
    if (sptr) {
      sptr->Stop();
    }
  }
}

TEST(EventLoop, AddTimer2) {
  system("rm -f /tmp/eventloop_test.log");
  vraft::LoggerOptions o;
  o.logger_name = "EventLoop.AddTimer2";
  vraft::vraft_logger.Init("/tmp/eventloop_test.log", o);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("test-loop");
  int32_t rv = loop->Init();
  ASSERT_EQ(rv, 0);

  vraft::TimerParam param;
  param.timeout_ms = 1000;
  param.repeat_ms = 1000;
  param.cb = TimerCb2;
  param.data = nullptr;
  param.repeat_times = 5;
  param.name = "test-timer";
  loop->AddTimer(param);

  std::thread t([loop]() { loop->Loop(); });
  loop->WaitStarted();
  t.join();

  std::cout << "loop stop" << std::endl;
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}