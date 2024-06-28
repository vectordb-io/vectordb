#ifndef VRAFT_INDEX_MANAGER_H_
#define VRAFT_INDEX_MANAGER_H_

#include <unordered_map>
#include <vector>

#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"

namespace vraft {

struct IndexItem {
  RaftIndex next;
  RaftIndex match;
};

class IndexManager final {
 public:
  IndexManager(const std::vector<RaftAddr> &peers);
  ~IndexManager();
  IndexManager(const IndexManager &t) = delete;
  IndexManager &operator=(const IndexManager &t) = delete;
  RaftIndex MajorityMax(RaftIndex leader_match_index);

  void Reset(const std::vector<RaftAddr> &peers);
  void ResetNext(RaftIndex index);
  void ResetMatch(RaftIndex index);

  void DecrNext(RaftAddr addr);
  void SetNext(RaftAddr addr, RaftIndex index);
  RaftIndex GetNext(RaftAddr addr);
  void SetMatch(RaftAddr addr, RaftIndex index);
  RaftIndex GetMatch(RaftAddr addr);

  void DecrNext(uint64_t addr);
  void SetNext(uint64_t addr, RaftIndex index);
  RaftIndex GetNext(uint64_t addr);
  void SetMatch(uint64_t addr, RaftIndex index);
  RaftIndex GetMatch(uint64_t addr);

 public:
  std::unordered_map<uint64_t, IndexItem> indices;

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

inline IndexManager::IndexManager(const std::vector<RaftAddr> &peers) {
  for (auto addr : peers) {
    IndexItem item;
    item.next = 1;
    item.match = 0;
    indices[addr.ToU64()] = item;
  }
}

inline IndexManager::~IndexManager() {}

inline RaftIndex IndexManager::MajorityMax(RaftIndex leader_match_index) {
  std::vector<RaftIndex> matches;
  matches.push_back(leader_match_index);
  for (auto &item : indices) {
    matches.push_back(item.second.match);
  }
  std::sort(matches.begin(), matches.end());
  return matches.at((matches.size() - 1) / 2);
}

inline void IndexManager::Reset(const std::vector<RaftAddr> &peers) {
  indices.clear();
  for (auto addr : peers) {
    IndexItem item;
    item.next = 1;
    item.match = 0;
    indices[addr.ToU64()] = item;
  }
}

inline void IndexManager::ResetNext(RaftIndex index) {
  for (auto &peer : indices) {
    peer.second.next = index;
  }
}

inline void IndexManager::ResetMatch(RaftIndex index) {
  for (auto &peer : indices) {
    peer.second.match = index;
  }
}

inline void IndexManager::DecrNext(RaftAddr addr) {
  indices[addr.ToU64()].next--;
}

inline void IndexManager::SetNext(RaftAddr addr, RaftIndex index) {
  indices[addr.ToU64()].next = index;
}

inline RaftIndex IndexManager::GetNext(RaftAddr addr) {
  return indices[addr.ToU64()].next;
}

inline void IndexManager::SetMatch(RaftAddr addr, RaftIndex index) {
  indices[addr.ToU64()].match = index;
}

inline RaftIndex IndexManager::GetMatch(RaftAddr addr) {
  return indices[addr.ToU64()].match;
}

inline void IndexManager::DecrNext(uint64_t addr) { indices[addr].next--; }

inline void IndexManager::SetNext(uint64_t addr, RaftIndex index) {
  indices[addr].next = index;
}

inline RaftIndex IndexManager::GetNext(uint64_t addr) {
  return indices[addr].next;
}

inline void IndexManager::SetMatch(uint64_t addr, RaftIndex index) {
  indices[addr].match = index;
}

inline RaftIndex IndexManager::GetMatch(uint64_t addr) {
  return indices[addr].match;
}

inline nlohmann::json IndexManager::ToJson() {
  nlohmann::json j;
  for (auto peer : indices) {
    RaftAddr addr(peer.first);
    j[addr.ToString()]["next"] = peer.second.next;
    j[addr.ToString()]["match"] = peer.second.match;
  }
  return j;
}

inline nlohmann::json IndexManager::ToJsonTiny() {
  nlohmann::json j;
  for (auto peer : indices) {
    RaftAddr addr(peer.first);
    j[addr.ToString()]["n"] = peer.second.next;
    j[addr.ToString()]["m"] = peer.second.match;
  }
  return j;
}

inline std::string IndexManager::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["idx_mgr"] = ToJsonTiny();
  } else {
    j["index_manager"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
