#ifndef VRAFT_RAFT_CONFIG_H_
#define VRAFT_RAFT_CONFIG_H_

#include <set>
#include <vector>

#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"

namespace vraft {

struct RaftConfig {
  RaftAddr me;
  std::vector<RaftAddr> peers;

  bool operator==(const RaftConfig& rhs) const;

  int32_t MaxBytes();
  int32_t ToString(std::string& s);
  int32_t ToString(const char* ptr, int32_t len);
  int32_t FromString(std::string& s);
  int32_t FromString(const char* ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  bool InConfig(const RaftAddr& addr);
  std::vector<RaftAddr> ToVector() const;
  std::set<RaftAddr> ToSet() const;
  bool IamIn(const RaftConfig& rhs) const;
};

using AppendConfigFunc = std::function<void(const RaftConfig& rc, RaftIndex i)>;
using DeleteConfigFunc = std::function<void(RaftIndex)>;

// return 0, c1 == c2
// return -1, c1 < c2 && c1 in c2
// return 1, c1 > c2 && c2 in c1
// return -2, c1 not in c2 && c2 not in c1
int32_t ConfigCompare(const RaftConfig& c1, const RaftConfig& c2,
                      std::vector<RaftAddr>& diff);

}  // namespace vraft

#endif
