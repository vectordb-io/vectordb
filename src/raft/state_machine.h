#ifndef VRAFT_STATE_MACHINE_H_
#define VRAFT_STATE_MACHINE_H_

#include <cstdint>
#include <memory>

#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"

namespace vraft {

using CreateSMFunc = std::function<StateMachineSPtr(std::string &path)>;

class LogEntry;

class StateMachine {
 public:
  StateMachine(std::string path) : path_(path) {}
  virtual ~StateMachine() {}
  StateMachine(const StateMachine &t) = delete;
  StateMachine &operator=(const StateMachine &t) = delete;

  virtual int32_t Restore() = 0;
  virtual int32_t Apply(LogEntry *entry, RaftAddr addr) = 0;
  virtual RaftIndex LastIndex() = 0;
  virtual RaftTerm LastTerm() = 0;

  virtual nlohmann::json ToJson() {
    nlohmann::json j;
    return j;
  }
  virtual nlohmann::json ToJsonTiny() {
    nlohmann::json j;
    return j;
  }
  virtual std::string ToJsonString(bool tiny, bool one_line) { return ""; }

  std::string path() { return path_; }

 public:
  SnapshotReaderSPtr reader_;
  SnapshotWriterSPtr writer_;

 protected:
  std::string path_;
};

}  // namespace vraft

#endif
