#ifndef VRAFT_LOGGER_H_
#define VRAFT_LOGGER_H_

#include <cassert>
#include <cstdarg>
#include <functional>
#include <memory>
#include <string>

#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

namespace vraft {

#define LOG_BUF_SIZE 8192
using LogFunc = std::function<void(const char *)>;

// use fatal instead of critical
// use debug, trace, info, error, fatal
enum LoggerLevel {
  kLoggerTrace = SPDLOG_LEVEL_TRACE,     // 0
  kLoggerDebug = SPDLOG_LEVEL_DEBUG,     // 1
  kLoggerInfo = SPDLOG_LEVEL_INFO,       // 2
  kLoggerWarn = SPDLOG_LEVEL_WARN,       // 3
  kLoggerError = SPDLOG_LEVEL_ERROR,     // 4
  kLoggerFatal = SPDLOG_LEVEL_CRITICAL,  // 5
  kLoggerOff = SPDLOG_LEVEL_OFF,         // 6
  kLoggerLevelNum
};

inline enum LoggerLevel U8ToLevel(uint8_t level) {
  switch (level) {
    case 0:
      return kLoggerTrace;
    case 1:
      return kLoggerDebug;
    case 2:
      return kLoggerInfo;
    case 3:
      return kLoggerWarn;
    case 4:
      return kLoggerError;
    case 5:
      return kLoggerFatal;
    case 6:
      return kLoggerOff;
    default:
      assert(0);
  }
}

struct LoggerOptions {
  std::string logger_name = "default";
  bool async = false;
  int32_t thread_num = 1;
  int32_t async_queue_size = 8192;
  LoggerLevel level = kLoggerTrace;
  bool enable_debug = false;
};

class Logger final {
 public:
  Logger(const std::string &file_name, const LoggerOptions &options);
  Logger();
  ~Logger();
  Logger(const Logger &t) = delete;
  Logger &operator=(const Logger &t) = delete;

  void Init(const std::string &file_name, const LoggerOptions &options);
  void SetLevel(LoggerLevel level);
  void Flush();

  void Trace(const char *format, ...);
  void Debug(const char *format, ...);
  void Info(const char *format, ...);
  void Warn(const char *format, ...);
  void Error(const char *format, ...);
  void Fatal(const char *format, ...);

  void FTrace(const char *format, ...);
  void FDebug(const char *format, ...);
  void FInfo(const char *format, ...);
  void FWarn(const char *format, ...);
  void FError(const char *format, ...);
  void FFatal(const char *format, ...);

  static void ShutDown() { spdlog::shutdown(); }

 private:
  void Init();
  void DoLog(const char *format, std::va_list arguments, LogFunc func);
  void DoLogTrace(const char *str);
  void DoLogDebug(const char *str);
  void DoLogInfo(const char *str);
  void DoLogWarn(const char *str);
  void DoLogError(const char *str);
  void DoLogFatal(const char *str);

 private:
  std::string file_name_;
  LoggerOptions options_;

  std::shared_ptr<spdlog::logger> logger_;
};

inline Logger::Logger(const std::string &file_name,
                      const LoggerOptions &options)
    : file_name_(file_name), options_(options) {
  Init();
}

inline Logger::Logger() {}

inline Logger::~Logger() {
  // spdlog::drop_all();
  // spdlog::shutdown();
}

inline void Logger::SetLevel(LoggerLevel level) {
  if (logger_) {
    logger_->set_level(static_cast<spdlog::level::level_enum>(level));
  }
}

inline void Logger::Flush() {
  if (logger_) {
    logger_->flush();
  }
}

inline void Logger::DoLogTrace(const char *str) {
  if (logger_) {
    logger_->trace(str);
  }
}

inline void Logger::DoLogDebug(const char *str) {
  if (options_.enable_debug) {
    if (logger_) {
      logger_->debug(str);
    }
  }
}

inline void Logger::DoLogInfo(const char *str) {
  if (logger_) {
    logger_->info(str);
  }
}

inline void Logger::DoLogWarn(const char *str) {
  if (logger_) {
    logger_->warn(str);
  }
}

inline void Logger::DoLogError(const char *str) {
  if (logger_) {
    logger_->error(str);
  }
}

inline void Logger::DoLogFatal(const char *str) {
  if (logger_) {
    logger_->critical(str);
  }
}

}  // namespace vraft

#endif
