#include "util.h"

#include <gtest/gtest.h>

#include <iostream>

namespace vectordb {

TEST(TimeStampTest, ToString) {
  TimeStamp ts;
  std::cout << "Seconds:\t" << ts.Seconds() << std::endl;
  std::cout << "MilliSeconds:\t" << ts.MilliSeconds() << std::endl;
  std::cout << "MicroSeconds:\t" << ts.MicroSeconds() << std::endl;
  std::cout << "NanoSeconds:\t" << ts.NanoSeconds() << std::endl;
  std::cout << "ToString:\t" << ts.ToString() << std::endl;
}

TEST(TimeStampTest, MilliSeconds) {
  TimeStamp t;
  int64_t ms = t.MilliSeconds();
  TimeStamp t2(ms);
  std::cout << "t:\t" << t.ToString() << std::endl;
  std::cout << "t2:\t" << t2.ToString() << std::endl;
  EXPECT_EQ(t.ToString(), t2.ToString());
}

}  // namespace vectordb