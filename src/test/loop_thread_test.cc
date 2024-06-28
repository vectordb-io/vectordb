#include "loop_thread.h"

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

vraft::LoopThread lt("test-loop-thread", false);

std::atomic<int32_t> num(0);
void Print(int32_t i) {
  num.fetch_add(1);
  // std::cout << "---" << i << std::endl;
}

#define PUSH_CNT 1000000
void Push() {
  for (int32_t i = 0; i < PUSH_CNT; ++i) {
    lt.RunFunctor(std::bind(&Print, i));
  }
}

void TimerCb(vraft::Timer *t) {
  std::cout << "TimerCb " << t->repeat_counter() << "..." << std::endl;
  t->RepeatDecr();
}

#define THREAD_CNT 10
TEST(LoopThread, test) {
  lt.Start();

  vraft::TimerParam param;
  param.cb = TimerCb;
  param.timeout_ms = 0;
  param.repeat_ms = 1000;
  param.repeat_times = 1000000;
  lt.AddTimer(param);

  std::vector<std::thread> threads;
  for (int32_t i = 0; i < THREAD_CNT; ++i) {
    std::cout << "start thread " << i << " ..." << std::endl;
    std::thread t(Push);
    threads.push_back(std::move(t));
  }

  for (auto &t : threads) {
    t.join();
  }
  threads.clear();

  lt.Stop();
  lt.Join();
  ASSERT_EQ(num.load(), THREAD_CNT * PUSH_CNT);
}

vraft::LoopThreadPool pool("pool", 5);
void PoolPush() {
  for (int32_t i = 0; i < PUSH_CNT; ++i) {
    pool.RunFunctor(i, std::bind(&Print, i));
  }
}

TEST(LoopThreadPool, test) {
  num.store(0);
  pool.Start();

  std::vector<std::thread> threads;
  for (int32_t i = 0; i < THREAD_CNT; ++i) {
    std::cout << "start thread " << i << " ..." << std::endl;
    std::thread t(PoolPush);
    threads.push_back(std::move(t));
  }

  for (auto &t : threads) {
    t.join();
  }
  threads.clear();

  pool.Stop();
  pool.Join();
  ASSERT_EQ(num.load(), THREAD_CNT * PUSH_CNT);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}