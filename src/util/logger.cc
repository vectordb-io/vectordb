#include "logger.h"

#include "util.h"

namespace vraft {

void Logger::Init(const std::string &file_name, const LoggerOptions &options) {
  file_name_ = file_name;
  options_ = options;
  Init();
}

void Logger::Init() {
  if (options_.async) {
    spdlog::init_thread_pool(options_.async_queue_size, options_.thread_num);
    logger_ = spdlog::basic_logger_mt<spdlog::async_factory>(
        options_.logger_name, file_name_);
  } else {
    logger_ = spdlog::basic_logger_mt(options_.logger_name, file_name_, true);
  }
  SetLevel(options_.level);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v");
}

void Logger::Trace(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogTrace, this, std::placeholders::_1));
  va_end(arguments);
}

void Logger::Debug(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogDebug, this, std::placeholders::_1));
  va_end(arguments);
}

void Logger::Info(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogInfo, this, std::placeholders::_1));
  va_end(arguments);
}

void Logger::Warn(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogWarn, this, std::placeholders::_1));
  va_end(arguments);
}

void Logger::Error(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogError, this, std::placeholders::_1));
  va_end(arguments);
}

void Logger::Fatal(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogFatal, this, std::placeholders::_1));
  va_end(arguments);

  assert(0);
}

void Logger::FTrace(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogTrace, this, std::placeholders::_1));
  va_end(arguments);

  Flush();
}

void Logger::FDebug(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogDebug, this, std::placeholders::_1));
  va_end(arguments);

  Flush();
}

void Logger::FInfo(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogInfo, this, std::placeholders::_1));
  va_end(arguments);

  Flush();
}

void Logger::FWarn(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogWarn, this, std::placeholders::_1));
  va_end(arguments);

  Flush();
}

void Logger::FError(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogError, this, std::placeholders::_1));
  va_end(arguments);

  Flush();
}

void Logger::FFatal(const char *format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  DoLog(format, arguments,
        std::bind(&Logger::DoLogFatal, this, std::placeholders::_1));
  va_end(arguments);

  Flush();
  assert(0);
}

void Logger::DoLog(const char *format, std::va_list arguments, LogFunc func) {
  char buf[LOG_BUF_SIZE];
  char *p_buf = buf;

  std::va_list arguments_copy;
  va_copy(arguments_copy, arguments);
  int32_t n = std::vsnprintf(buf, sizeof(buf), format, arguments_copy);
  va_end(arguments_copy);

  if (n >= LOG_BUF_SIZE) {
    p_buf = new char[n + 1];
    std::vsnprintf(p_buf, n + 1, format, arguments_copy);
    func(p_buf);
    delete[] p_buf;

  } else {
    func(p_buf);
  }
}

}  // namespace vraft
