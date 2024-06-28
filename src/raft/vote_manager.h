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
};

class VoteManager final {
 public:
  VoteManager(const std::vector<RaftAddr> &peers);
  ~VoteManager();
  VoteManager(const VoteManager &t) = delete;
  VoteManager &operator=(const VoteManager &t) = delete;

  void Reset(const std::vector<RaftAddr> &peers);
  bool Majority(bool my_vote);
  bool QuorumAll(bool my_vote);
  void Clear();
  void GetVote(uint64_t id);
  void Done(uint64_t id);

 public:
  std::unordered_map<uint64_t, VoteItem> votes;

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

inline VoteManager::VoteManager(const std::vector<RaftAddr> &peers) {
  for (auto addr : peers) {
    VoteItem vote_item = {false, false};
    votes[addr.ToU64()] = vote_item;
  }
}

inline VoteManager::~VoteManager() {}

inline void VoteManager::Reset(const std::vector<RaftAddr> &peers) {
  votes.clear();
  VoteItem vote_item = {false, false};
  for (auto addr : peers) {
    votes[addr.ToU64()] = vote_item;
  }
}

inline bool VoteManager::Majority(bool my_vote) {
  int32_t vote_count = 0;
  if (my_vote) {
    ++vote_count;
  }
  for (auto &v : votes) {
    if (v.second.grant == true) {
      ++vote_count;
    }
  }
  return (vote_count >= (static_cast<int32_t>(votes.size() + 1 + 1) / 2));
}

inline bool VoteManager::QuorumAll(bool my_vote) {
  if (!my_vote) {
    return false;
  }
  for (auto &v : votes) {
    if (v.second.grant == false) {
      return false;
    }
  }
  return true;
}

inline void VoteManager::Clear() {
  for (auto &v : votes) {
    v.second.grant = false;
    v.second.done = false;
  }
}

inline void VoteManager::GetVote(uint64_t id) { votes[id].grant = true; }

inline void VoteManager::Done(uint64_t id) { votes[id].done = true; }

inline nlohmann::json VoteManager::ToJson() {
  nlohmann::json j;
  for (auto peer : votes) {
    RaftAddr addr(peer.first);
    j[addr.ToString()]["grant"] = peer.second.grant;
    j[addr.ToString()]["done"] = peer.second.done;
  }
  return j;
}

inline nlohmann::json VoteManager::ToJsonTiny() {
  nlohmann::json j;
  for (auto peer : votes) {
    RaftAddr addr(peer.first);
    j[addr.ToString()]["gr"] = peer.second.grant;
    j[addr.ToString()]["dn"] = peer.second.done;
  }
  return j;
}

inline std::string VoteManager::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vt_mgr"] = ToJsonTiny();
  } else {
    j["vote_manager"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
