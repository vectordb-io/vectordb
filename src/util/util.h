#ifndef VECTORDB_UTIL_UTIL_H
#define VECTORDB_UTIL_UTIL_H

#include <chrono>
#include <string>
#include <vector>

namespace vectordb {

class TimeStamp {
 public:
  TimeStamp();
  TimeStamp(int64_t milliseconds);
  ~TimeStamp();

  int64_t Seconds();
  int64_t MilliSeconds();
  int64_t MicroSeconds();
  int64_t NanoSeconds();
  std::string ToString() const;

 private:
  std::chrono::time_point<std::chrono::system_clock> start_time_;
};

}  // namespace vectordb

#endif