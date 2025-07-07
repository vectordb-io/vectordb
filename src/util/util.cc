#include "util.h"

#include <chrono>

namespace vectordb {

TimeStamp::TimeStamp() { start_time_ = std::chrono::system_clock::now(); }

TimeStamp::TimeStamp(int64_t milliseconds) {
  start_time_ = std::chrono::system_clock::time_point(
      std::chrono::milliseconds(milliseconds));
}

TimeStamp::~TimeStamp() {}

int64_t TimeStamp::Seconds() {
  auto duration = start_time_.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

int64_t TimeStamp::MilliSeconds() {
  auto duration = start_time_.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(duration)
      .count();
}

int64_t TimeStamp::MicroSeconds() {
  auto duration = start_time_.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::microseconds>(duration)
      .count();
}

int64_t TimeStamp::NanoSeconds() {
  auto duration = start_time_.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

// 格式：yyyy-MM-dd HH:mm:ss.ms
std::string TimeStamp::ToString() const {
  auto now = start_time_;
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count() %
            1000;

  struct tm tm_time;
  localtime_r(&time_t, &tm_time);

  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
           tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
           tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, ms);

  return std::string(buffer);
}

}  // namespace vectordb