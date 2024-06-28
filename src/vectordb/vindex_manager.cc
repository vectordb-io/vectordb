#include "vindex_manager.h"

#include <map>

#include "common.h"
#include "util.h"
#include "vengine.h"

namespace vectordb {

VindexManager::VindexManager() {}

bool VindexManager::HasIndex() const { return indices_.size() > 0; }

int32_t VindexManager::Add(VindexSPtr index) {
  indices_[index->param().timestamp] = index;
  return 0;
}

int32_t VindexManager::Del(uint64_t timestamp) {
  indices_.erase(timestamp);
  return 0;
}

VindexSPtr VindexManager::Get(uint64_t timestamp) {
  VindexSPtr sptr = nullptr;
  auto it = indices_.find(timestamp);
  if (it != indices_.end()) {
    sptr = it->second;
  }
  return sptr;
}

VindexSPtr VindexManager::GetNewest() {
  VindexSPtr sptr = nullptr;
  if (indices_.size() > 0) {
    auto it = indices_.rbegin();
    sptr = it->second;
  }
  return sptr;
}

nlohmann::json VindexManager::ToJson() {
  nlohmann::json j;
  int32_t i = 0;
  for (auto &kv : indices_) {
    j[i]["index"] = kv.second->ToJson();
    j[i]["time"] = vraft::NsToString(kv.first);
    ++i;
  }
  return j;
}

nlohmann::json VindexManager::ToJsonTiny() { return ToJson(); }

std::string VindexManager::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["indices"] = ToJsonTiny();
  } else {
    j["indices"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vectordb
