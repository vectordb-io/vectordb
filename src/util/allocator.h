#ifndef VRAFT_ALLOCATOR_H_
#define VRAFT_ALLOCATOR_H_

#include <cstdlib>

namespace vraft {

class Allocator final {
 public:
  Allocator() {}
  ~Allocator() {}

  Allocator(const Allocator &t) = delete;
  Allocator &operator=(const Allocator &t) = delete;

  void *Malloc(size_t size);
  void Free(void *ptr);

 private:
  void *mem_;
};

Allocator &DefaultAllocator();

}  // namespace vraft

#endif
