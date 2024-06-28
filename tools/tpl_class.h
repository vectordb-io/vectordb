#ifndef VRAFT_TPL_H_
#define VRAFT_TPL_H_

namespace vraft {

class Tpl final {
public:
  explicit Tpl();
  ~Tpl();
  Tpl(const Tpl &) = delete;
  Tpl &operator=(const Tpl &) = delete;

private:
};

inline Tpl::Tpl() {}

inline Tpl::~Tpl() {}

} // namespace vraft

#endif
