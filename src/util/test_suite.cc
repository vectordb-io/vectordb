#include "test_suite.h"

#include "raft.h"
#include "timer.h"

namespace vraft {

TimerFunctor timer_func;
TestState current_state = kTestState0;
std::unordered_map<TestState, StateChange> rules;

bool HasLeader() { return true; }

void InitRemuTest() {
  rules[kTestState0].next = kTestStateEnd;
  rules[kTestState0].func = HasLeader;
}

}  // namespace vraft
