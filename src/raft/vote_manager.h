#ifndef VRAFT_VOTE_MANAGER_H_
#define VRAFT_VOTE_MANAGER_H_

#include <unordered_map>
#include <vector>

#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"

namespace vraft {

struct VoteItem {
  bool grant;
  bool done;
  bool logok;
  bool interval_ok;
};

class VoteManager final {
 public:
  VoteManager(const std::vector<RaftAddr> &peers);
  ~VoteManager();
  VoteManager(const VoteManager &t) = delete;
  VoteManager &operator=(const VoteManager &t) = delete;

  void Reset(const std::vector<RaftAddr> &peers);
  bool Majority(bool my_vote);
  bool MajorityLogOK(bool my_vote);
  bool MajorityPreVoteOK(bool my_vote);
  bool QuorumAll(bool my_vote);
  void Clear();
  void GetVote(uint64_t id);
  void Done(uint64_t id);
  void LogOK(uint64_t id);
  void IntervalOK(uint64_t id);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

 public:
  std::unordered_map<uint64_t, VoteItem> votes;
};

}  // namespace vraft

#endif
