#include "count_down.h"

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

vraft::CountDownLatch latch(1);
TEST(CountDownLatch, test) {
  std::thread t([] {
    latch.Wait();
    std::cout << "t run ..." << std::endl;
  });

  std::thread t2([] {
    latch.Wait();
    std::cout << "t2 run ..." << std::endl;
  });

  std::thread t3([] {
    std::cout << "t3 sleep 3s, count down" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    latch.CountDown();
  });

  t.join();
  t2.join();
  t3.join();
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}