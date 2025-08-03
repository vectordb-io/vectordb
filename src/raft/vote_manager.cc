#include "vote_manager.h"

namespace vraft {

VoteManager::VoteManager(const std::vector<RaftAddr> &peers) { Reset(peers); }

VoteManager::~VoteManager() {}

void VoteManager::Reset(const std::vector<RaftAddr> &peers) {
  votes.clear();
  VoteItem vote_item = {false, false, false, false};
  for (auto addr : peers) {
    votes[addr.ToU64()] = vote_item;
  }
}

bool VoteManager::Majority(bool my_vote) {
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

bool VoteManager::MajorityLogOK(bool my_vote) {
  int32_t vote_count = 0;
  if (my_vote) {
    ++vote_count;
  }
  for (auto &v : votes) {
    if (v.second.logok == true) {
      ++vote_count;
    }
  }
  return (vote_count >= (static_cast<int32_t>(votes.size() + 1 + 1) / 2));
}

bool VoteManager::MajorityPreVoteOK(bool my_vote) {
  int32_t vote_count = 0;
  if (my_vote) {
    ++vote_count;
  }
  for (auto &v : votes) {
    if (v.second.logok && v.second.interval_ok) {
      ++vote_count;
    }
  }
  return (vote_count >= (static_cast<int32_t>(votes.size() + 1 + 1) / 2));
}

bool VoteManager::QuorumAll(bool my_vote) {
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

void VoteManager::Clear() {
  for (auto &v : votes) {
    v.second.grant = false;
    v.second.done = false;
    v.second.logok = false;
    v.second.interval_ok = false;
  }
}

void VoteManager::GetVote(uint64_t id) { votes[id].grant = true; }

void VoteManager::Done(uint64_t id) { votes[id].done = true; }

void VoteManager::LogOK(uint64_t id) { votes[id].logok = true; }

void VoteManager::IntervalOK(uint64_t id) { votes[id].interval_ok = true; }

nlohmann::json VoteManager::ToJson() {
  nlohmann::json j;
  for (auto peer : votes) {
    RaftAddr addr(peer.first);
    j[addr.ToString()]["grant"] = peer.second.grant;
    j[addr.ToString()]["done"] = peer.second.done;
    j[addr.ToString()]["logok"] = peer.second.logok;
    j[addr.ToString()]["interval-ok"] = peer.second.interval_ok;
  }
  return j;
}

nlohmann::json VoteManager::ToJsonTiny() {
  nlohmann::json j;
  for (auto peer : votes) {
    RaftAddr addr(peer.first);
    j[addr.ToString()]["gr"] = peer.second.grant;
    j[addr.ToString()]["dn"] = peer.second.done;
    j[addr.ToString()]["lok"] = peer.second.logok;
    j[addr.ToString()]["iok"] = peer.second.interval_ok;
  }
  return j;
}

std::string VoteManager::ToJsonString(bool tiny, bool one_line) {
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
