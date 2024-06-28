#ifndef VRAFT_COUNT_DOWN_H_
#define VRAFT_COUNT_DOWN_H_

#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace vraft {

class CountDownLatch final {
 public:
  CountDownLatch(int32_t count);
  ~CountDownLatch();
  CountDownLatch(const CountDownLatch &t) = delete;
  CountDownLatch &operator=(const CountDownLatch &t) = delete;

  void Wait();
  void Reset();
  void CountDown();

  int32_t count() const;

 private:
  std::mutex mu_;
  std::condition_variable cond_var_;

  int32_t count_;
  int32_t count_imm_;
};

inline CountDownLatch::CountDownLatch(int32_t count)
    : count_(count), count_imm_(count) {
  assert(count >= 0);
}

inline CountDownLatch::~CountDownLatch() {}

inline int32_t CountDownLatch::count() const { return count_; }

}  // namespace vraft

#endif
