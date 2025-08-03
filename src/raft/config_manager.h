#ifndef VRAFT_CONFIG_MANAGER_H_
#define VRAFT_CONFIG_MANAGER_H_

#include <vector>

#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_config.h"

namespace vraft {

class ConfigManager final {
 public:
  ConfigManager(const RaftConfig& current);
  ~ConfigManager();
  ConfigManager(const ConfigManager& t) = delete;
  ConfigManager& operator=(const ConfigManager& t) = delete;

  RaftConfigSPtr Previous();
  RaftConfigSPtr Current();
  void SetCurrent(const RaftConfig& rc);
  void Rollback();

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  void set_current_cb(Functor cb);
  void RunCb();

 private:
  RaftConfigSPtr current_;
  RaftConfigSPtr previous_;

  Functor current_cb_;
};

inline ConfigManager::ConfigManager(const RaftConfig& current) {
  current_ = std::make_shared<RaftConfig>();
  *current_ = current;

  previous_ = nullptr;
  current_cb_ = nullptr;
}

inline ConfigManager::~ConfigManager() {}

inline RaftConfigSPtr ConfigManager::Previous() { return previous_; }

inline RaftConfigSPtr ConfigManager::Current() { return current_; }

inline void ConfigManager::SetCurrent(const RaftConfig& rc) {
  if (current_) {
    previous_ = current_;
  }
  current_ = std::make_shared<RaftConfig>();
  *current_ = rc;

  RunCb();
}

inline void ConfigManager::Rollback() {
  assert(previous_);
  current_ = previous_;
  previous_ = nullptr;

  RunCb();
}

inline nlohmann::json ConfigManager::ToJson() {
  nlohmann::json j;
  if (current_) {
    j["cur"] = current_->ToJson();
  } else {
    j["cur"] = "null";
  }
  if (previous_) {
    j["pre"] = previous_->ToJson();
  } else {
    j["pre"] = "null";
  }
  return j;
}

inline nlohmann::json ConfigManager::ToJsonTiny() {
  nlohmann::json j;
  if (current_) {
    j["cur"] = current_->ToJsonTiny();
  } else {
    j["cur"] = "null";
  }
  if (previous_) {
    j["pre"] = previous_->ToJsonTiny();
  } else {
    j["pre"] = "null";
  }
  return j;
}

inline std::string ConfigManager::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["cf_mgr"] = ToJsonTiny();
  } else {
    j["config_manager"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
