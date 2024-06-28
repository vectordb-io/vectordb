#include "allocator.h"

namespace vraft {

void *Allocator::Malloc(size_t size) { return malloc(size); }

void Allocator::Free(void *ptr) { free(ptr); }

Allocator default_allocator;
Allocator &DefaultAllocator() { return default_allocator; }

}  // namespace vraft
