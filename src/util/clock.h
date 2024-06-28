#ifndef VRAFT_CLOCK_H_
#define VRAFT_CLOCK_H_

#include <chrono>
#include <string>

namespace vraft {

class Clock final {
 public:
  Clock() {}
  ~Clock() {}

  Clock(const Clock &c) = delete;
  Clock &operator=(const Clock &c) = delete;

  static int64_t Sec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  static int64_t MSec() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  static int64_t USec() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  static int64_t NSec() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  static std::string NSecStr();

 private:
};

class SteadyClock final {
 public:
  SteadyClock() {}
  ~SteadyClock() {}

  SteadyClock(const SteadyClock &c) = delete;
  SteadyClock &operator=(const SteadyClock &c) = delete;

  static int64_t Sec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  static int64_t MSec() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  static int64_t USec() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  static int64_t NSec() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

 private:
};

}  // namespace vraft

#endif
