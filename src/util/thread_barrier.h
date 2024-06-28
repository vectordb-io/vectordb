#ifndef VRAFT_BARRIER_H_
#define VRAFT_BARRIER_H_

#include <pthread.h>

#include <cstdint>

namespace vraft {

class Barrier final {
 public:
  Barrier(int32_t num);
  ~Barrier();
  Barrier(const Barrier &) = delete;
  Barrier &operator=(const Barrier &) = delete;

  int32_t num() const { return num_; }
  int32_t ArriveAndWait();

 private:
  int32_t num_;
  pthread_barrier_t b_;
};

}  // namespace vraft

#endif
