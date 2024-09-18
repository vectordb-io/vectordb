#ifndef VRAFT_REMU_H_
#define VRAFT_REMU_H_

#include <vector>

#include "common.h"
#include "config.h"
#include "state_machine.h"

namespace vraft {

// raft emulator, good!!
struct Remu {
  Remu(EventLoopSPtr &l, bool enable_pre_vote, bool interval_check)
      : loop(l),
        enable_pre_vote_(enable_pre_vote),
        interval_check_(interval_check) {}

  EventLoopWPtr loop;
  std::vector<vraft::Config> configs;
  std::vector<vraft::RaftServerSPtr> raft_servers;
  std::vector<vraft::StateMachineSPtr> state_machines;

  TracerCb tracer_cb;
  CreateSMFunc create_sm;

  int32_t LeaderTimes();

  void Create();
  void Start();
  void Stop();
  void Clear();
  void AddOneNode();

  void Log(std::string key);
  void Print(bool tiny = true, bool one_line = true);
  void PrintConfig();

  void Check();

 private:
  void CheckLeader();
  void CheckLog();
  void CheckMeta();
  void CheckIndex();

 private:
  bool enable_pre_vote_;
  bool interval_check_;
};

}  // namespace vraft

#endif
