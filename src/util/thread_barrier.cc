#include "thread_barrier.h"

#include <cassert>

namespace vraft {

Barrier::Barrier(int32_t num) : num_(num) {
  assert(num > 0);
  pthread_barrier_init(&b_, nullptr, num);
}

Barrier::~Barrier() { pthread_barrier_destroy(&b_); }

int32_t Barrier::ArriveAndWait() { return pthread_barrier_wait(&b_); }

}  // namespace vraft
