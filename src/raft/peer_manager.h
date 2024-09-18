#ifndef VRAFT_PEER_MANAGER_H_
#define VRAFT_PEER_MANAGER_H_

#include <unordered_map>
#include <vector>

#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"

namespace vraft {

struct PeerItem {
  bool pre_voting;
};

class PeerManager final {
 public:
  explicit PeerManager(const std::vector<RaftAddr> &peers);
  ~PeerManager();
  PeerManager(const PeerManager &) = delete;
  PeerManager &operator=(const PeerManager &) = delete;

  void Reset(const std::vector<RaftAddr> &peers);
  void Clear();
  void ClearPreVoting();
  void SetPreVoting();

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

 public:
  std::unordered_map<uint64_t, PeerItem> items;
};

}  // namespace vraft

#endif
