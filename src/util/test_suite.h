#ifndef VRAFT_TEST_SUITE_H_
#define VRAFT_TEST_SUITE_H_

#include <functional>
#include <unordered_map>

#include "timer.h"

namespace vraft {

class Timer;
using CondFunc = std::function<bool()>;

enum TestState {
  kTestState0 = 0,
  kTestState1,
  kTestState2,
  kTestState3,
  kTestState4,
  kTestState5,
  kTestState6,
  kTestState7,
  kTestState8,
  kTestState9,
  kTestStateEnd,
};

struct StateChange {
  TestState next;
  CondFunc func;
};

extern TestState current_state;
extern std::unordered_map<TestState, StateChange> rules;

}  // namespace vraft

#endif
