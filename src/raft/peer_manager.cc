#include "peer_manager.h"

namespace vraft {

PeerManager::PeerManager(const std::vector<RaftAddr> &peers) { Reset(peers); }

PeerManager::~PeerManager() {}

void PeerManager::Reset(const std::vector<RaftAddr> &peers) {
  items.clear();
  PeerItem item = {false};
  for (auto addr : peers) {
    items[addr.ToU64()] = item;
  }
}

void PeerManager::Clear() { ClearPreVoting(); }

void PeerManager::ClearPreVoting() {
  for (auto &v : items) {
    v.second.pre_voting = false;
  }
}

void PeerManager::SetPreVoting() {
  for (auto &v : items) {
    v.second.pre_voting = true;
  }
}

nlohmann::json PeerManager::ToJson() {
  nlohmann::json j;
  for (auto peer : items) {
    RaftAddr addr(peer.first);
    j[addr.ToString()]["pre-voting"] = peer.second.pre_voting;
  }
  return j;
}

nlohmann::json PeerManager::ToJsonTiny() {
  nlohmann::json j;
  for (auto peer : items) {
    RaftAddr addr(peer.first);
    j[addr.ToString()]["pvting"] = peer.second.pre_voting;
  }
  return j;
}

std::string PeerManager::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["pr_mgr"] = ToJsonTiny();
  } else {
    j["peer_manager"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft
