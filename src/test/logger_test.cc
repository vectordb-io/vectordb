#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "spdlog/spdlog.h"
#include "vraft_logger.h"

TEST(Logger, test) {
  system("rm -f /tmp/logger_test.log");
  vraft::LoggerOptions logger_options{"logger-test", false, 1, 8192,
                                      vraft::kLoggerTrace};
  logger_options.level = vraft::U8ToLevel(0);
  logger_options.enable_debug = true;
  vraft::vraft_logger.Init("/tmp/logger_test.log", logger_options);
  int a = 99;
  vraft::vraft_logger.FInfo("%s, %d", "info log test", a);

  vraft::Logger::ShutDown();
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}