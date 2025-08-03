#ifndef VRAFT_CHECKER_H_
#define VRAFT_CHECKER_H_

namespace vraft {

class Checker final {
 public:
  Checker();
  ~Checker();
  Checker(const Checker &t) = delete;
  Checker &operator=(const Checker &t) = delete;

 private:
};

inline Checker::Checker() {}

inline Checker::~Checker() {}

}  // namespace vraft

#endif
