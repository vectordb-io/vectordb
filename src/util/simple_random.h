#ifndef VRAFT_SIMPLE_RANDOM_H_
#define VRAFT_SIMPLE_RANDOM_H_

#include <cassert>
#include <cstdint>
#include <random>

namespace vraft {

class SimpleRandom final {
 public:
  SimpleRandom(uint32_t begin, uint32_t end);
  ~SimpleRandom();
  SimpleRandom(const SimpleRandom &t) = delete;
  SimpleRandom &operator=(const SimpleRandom &t) = delete;

  uint32_t Get() { return distrib_(gen_); }

 private:
  uint32_t begin_;
  uint32_t end_;

  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_int_distribution<> distrib_;
};

inline SimpleRandom::SimpleRandom(uint32_t begin, uint32_t end)
    : begin_(begin), end_(end), gen_(rd_()), distrib_(begin_, end_) {
  assert(end_ >= begin);
}

inline SimpleRandom::~SimpleRandom() {}

}  // namespace vraft

#endif
