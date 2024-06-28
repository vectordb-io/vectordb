#include "clock.h"

#include "util.h"

namespace vraft {

std::string Clock::NSecStr() { return NsToString(NSec()); }

}  // namespace vraft
