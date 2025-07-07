#include "logger.h"

#include <gtest/gtest.h>

#include <cstdio>  // 用于文件操作
#include <fstream>
#include <string>

namespace vectordb {

class LoggerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 创建临时日志文件
    log_file_ = "test_logger.log";
    // 确保测试开始时日志文件不存在
    std::remove(log_file_.c_str());
  }

  void TearDown() override {
    // 清理测试后的日志文件
    std::remove(log_file_.c_str());
  }

  std::string log_file_;
};

TEST_F(LoggerTest, InitLoggerCreatesLogFile) {
  // 测试初始化日志器会创建日志文件
  InitLogger(log_file_);

  // 检查日志文件是否被创建
  std::ifstream file(log_file_);
  EXPECT_TRUE(file.good());
  file.close();

  // 清理
  DestroyLogger();
}

TEST_F(LoggerTest, LoggingWorks) {
  // 初始化日志器
  InitLogger(log_file_);

  // 记录一些日志
  spdlog::get(kLoggerName)->info("Test log message");
  spdlog::get(kLoggerName)->error("Test error message");

  // 确保日志被写入
  DestroyLogger();

  // 读取日志文件内容
  std::ifstream log_stream(log_file_);
  std::string log_content((std::istreambuf_iterator<char>(log_stream)),
                          std::istreambuf_iterator<char>());

  // 验证日志内容
  EXPECT_TRUE(log_content.find("Test log message") != std::string::npos);
  EXPECT_TRUE(log_content.find("Test error message") != std::string::npos);
}

TEST_F(LoggerTest, DisableEnableLog) {
  // 初始化日志器
  InitLogger(log_file_);

  // 记录启用状态下的日志
  spdlog::get(kLoggerName)->info("Log when enabled");

  // 禁用日志
  DisableLog();
  spdlog::get(kLoggerName)->info("Log when disabled");

  // 重新启用日志
  EnableLog();
  spdlog::get(kLoggerName)->info("Log when re-enabled");

  // 确保日志被写入
  DestroyLogger();

  // 读取日志文件内容
  std::ifstream log_stream(log_file_);
  std::string log_content((std::istreambuf_iterator<char>(log_stream)),
                          std::istreambuf_iterator<char>());

  // 验证日志内容 - 禁用时的日志不应该出现
  EXPECT_TRUE(log_content.find("Log when enabled") != std::string::npos);
  EXPECT_TRUE(log_content.find("Log when disabled") == std::string::npos);
  EXPECT_TRUE(log_content.find("Log when re-enabled") != std::string::npos);
}

// 使用logger变量测试
TEST_F(LoggerTest, GlobalLoggerVariable) {
  // 初始化日志器
  InitLogger(log_file_);

  // 确认全局logger变量已被初始化
  EXPECT_TRUE(logger != nullptr);

  // 使用全局logger变量记录日志
  logger->info("Test global logger variable");
  logger->warn("Warning from global logger");

  // 禁用日志
  DisableLog();
  logger->error("This error should not appear");

  // 启用日志
  EnableLog();
  logger->critical("Critical message after re-enabling");

  // 确保日志被写入
  DestroyLogger();

  // 读取日志文件内容
  std::ifstream log_stream(log_file_);
  std::string log_content((std::istreambuf_iterator<char>(log_stream)),
                          std::istreambuf_iterator<char>());

  // 验证日志内容
  EXPECT_TRUE(log_content.find("Test global logger variable") !=
              std::string::npos);
  EXPECT_TRUE(log_content.find("Warning from global logger") !=
              std::string::npos);
  EXPECT_TRUE(log_content.find("This error should not appear") ==
              std::string::npos);
  EXPECT_TRUE(log_content.find("Critical message after re-enabling") !=
              std::string::npos);
}

}  // namespace vectordb

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
