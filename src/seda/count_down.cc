#include "count_down.h"

namespace vraft {

void CountDownLatch::Wait() {
  std::unique_lock<std::mutex> ulk(mu_);
  cond_var_.wait(ulk, [this] { return (this->count_ == 0); });
  assert(count_ == 0);
}

void CountDownLatch::Reset() {
  std::unique_lock<std::mutex> ulk(mu_);
  count_ = count_imm_;
}

void CountDownLatch::CountDown() {
  std::unique_lock<std::mutex> ulk(mu_);
  --count_;
  if (count_ == 0) {
    cond_var_.notify_all();
  }
}

}  // namespace vraft
