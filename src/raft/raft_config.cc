#include "raft_config.h"

#include "common.h"

namespace vraft {

nlohmann::json RaftConfig::ToJson() {
  nlohmann::json j;
  j["me"][0] = me.ToU64();
  j["me"][1] = me.ToString();
  int32_t i = 0;
  for (auto peer : peers) {
    j["peers"][i][0] = peer.ToU64();
    j["peers"][i][1] = peer.ToString();
    i++;
  }
  return j;
}

nlohmann::json RaftConfig::ToJsonTiny() {
  nlohmann::json j;
  j[0] = me.ToString();
  int32_t i = 1;
  for (auto peer : peers) {
    j[i++] = peer.ToString();
  }
  return j;
}

#if 0
nlohmann::json RaftConfig::ToJsonTiny() {
  nlohmann::json j;
  j["me"] = me.ToString();
  int32_t i = 0;
  for (auto peer : peers) {
    j["peers"][i++] = peer.ToString();
  }
  return j;
}
#endif

std::string RaftConfig::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["rc"] = ToJsonTiny();
  } else {
    j["raft_config"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

bool RaftConfig::InConfig(const RaftAddr &addr) {
  if (me.ToU64() == addr.ToU64()) {
    return true;
  }

  for (auto a : peers) {
    if (a.ToU64() == addr.ToU64()) {
      return true;
    }
  }

  return false;
}

std::vector<RaftAddr> RaftConfig::ToVector() const {
  std::vector<RaftAddr> all;
  all.push_back(me);
  for (auto peer : peers) {
    all.push_back(peer);
  }
  return all;
}

std::set<RaftAddr> RaftConfig::ToSet() const {
  std::set<RaftAddr> all;
  all.insert(me);
  for (auto peer : peers) {
    all.insert(peer);
  }
  return all;
}

bool RaftConfig::IamIn(const RaftConfig &rhs) const {
  std::vector<RaftAddr> my_addrs = ToVector();
  std::set<RaftAddr> your_addrs = rhs.ToSet();
  for (auto my_addr : my_addrs) {
    auto it = your_addrs.find(my_addr);
    if (it == your_addrs.end()) {
      return false;
    }
  }

  return true;
}

// return 0, c1 == c2
// return -1, c1 < c2 && c1 in c2
// return 1, c1 > c2 && c2 in c1
// return -2, c1 not in c2 && c2 not in c1
int32_t ConfigCompare(const RaftConfig &c1, const RaftConfig &c2,
                      std::vector<RaftAddr> &diff) {
  diff.clear();
  if (c1 == c2) {
    return 0;

  } else if (c1.IamIn(c2)) {
    std::set<RaftAddr> tmp_set = c1.ToSet();
    std::vector<RaftAddr> tmp_vec = c2.ToVector();
    for (auto addr : tmp_vec) {
      auto it = tmp_set.find(addr);
      if (it == tmp_set.end()) {
        diff.push_back(addr);
      }
    }
    return -1;

  } else if (c2.IamIn(c1)) {
    std::set<RaftAddr> tmp_set = c2.ToSet();
    std::vector<RaftAddr> tmp_vec = c1.ToVector();
    for (auto addr : tmp_vec) {
      auto it = tmp_set.find(addr);
      if (it == tmp_set.end()) {
        diff.push_back(addr);
      }
    }
    return 1;
  }

  return -2;
}

}  // namespace vraft
