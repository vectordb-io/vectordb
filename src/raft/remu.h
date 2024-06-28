#ifndef VRAFT_REMU_H_
#define VRAFT_REMU_H_

#include <vector>

#include "common.h"
#include "config.h"
#include "state_machine.h"

namespace vraft {

// raft emulator, good!!
struct Remu {
  Remu(EventLoopSPtr &l) : loop(l) {}

  EventLoopWPtr loop;
  std::vector<vraft::Config> configs;
  std::vector<vraft::RaftServerSPtr> raft_servers;
  std::vector<vraft::StateMachineSPtr> state_machines;

  TracerCb tracer_cb;
  CreateSMFunc create_sm;

  void Create();
  void Start();
  void Stop();
  void Clear();

  void Log(std::string key);
  void Print(bool tiny = true, bool one_line = true);
};

}  // namespace vraft

#endif
