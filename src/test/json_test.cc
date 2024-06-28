#include "nlohmann/json.hpp"

#include <gtest/gtest.h>

#include <csignal>
#include <fstream>
#include <iostream>

#include "util.h"

TEST(json, test) {
  nlohmann::json j;
  j["name"] = "cm";
  j["age"] = 8;
  std::cout << j.dump(4) << std::endl;
  std::cout << j.dump() << std::endl;
  std::string s = j.dump();
  std::cout << s << std::endl;
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}