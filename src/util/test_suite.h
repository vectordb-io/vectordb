#ifndef VRAFT_TEST_SUITE_H_
#define VRAFT_TEST_SUITE_H_

#include <functional>
#include <unordered_map>

#include "leveldb/db.h"
#include "leveldb/write_batch.h"
#include "nlohmann/json.hpp"
#include "state_machine.h"
#include "timer.h"

namespace vraft {

extern EventLoopSPtr gtest_loop;
extern RemuSPtr gtest_remu;
extern std::string gtest_path;

extern uint16_t normal_port;
extern uint16_t standby_port;

extern bool gtest_enable_pre_vote;
extern bool gtest_interval_check;
extern bool gtest_desc;
extern int32_t gtest_node_num;

void RemuParseConfig(int argc, char **argv);
void RemuLogState(std::string key);
void PrintAndCheck();
void GenerateConfig(std::vector<Config> &configs, int32_t peers_num);
void GTestSignalHandler(int signal);

using GTestTickFunc = std::function<void(Timer *)>;

void RemuTestSetUp(std::string path, GTestTickFunc tick_func,
                   CreateSMFunc create_sm);
void RemuTestTearDown();
void RunRemuTest(int32_t node_num);
void RunRemuTest2(int32_t node_num, int32_t add_node_num);

//------------------TestSM---------------------------

class TestSM : public vraft::StateMachine {
 public:
  TestSM(std::string path);
  ~TestSM();

  int32_t Restore() override;
  int32_t Apply(vraft::LogEntry *entry, vraft::RaftAddr addr) override;
  vraft::RaftIndex LastIndex() override;
  vraft::RaftTerm LastTerm() override;

  int32_t Get(const std::string &key, std::string &value);

  int32_t apply_count() { return apply_count_; }
  uint32_t check_sum() { return check_sum_; }
  std::string all_values() { return all_values_; }

  nlohmann::json ToJson() override;
  nlohmann::json ToJsonTiny() override;
  std::string ToJsonString(bool tiny, bool one_line) override;

 private:
  int32_t SetApplyCount(int32_t apply_count);
  int32_t GetApplyCount(int32_t &apply_count);
  int32_t SetCheckSum(int32_t check_sum);
  int32_t GetCheckSum(int32_t &check_sum);
  int32_t SetAllValues(const std::string &value);
  int32_t GetAllValues(std::string *value);

  int32_t SetI32(const std::string &key, int32_t i32);
  int32_t GetI32(const std::string &key, int32_t &i32);
  int32_t SetU32(const std::string &key, uint32_t u32);
  int32_t GetU32(const std::string &key, uint32_t &u32);
  int32_t SetU64(const std::string &key, uint64_t u64);
  int32_t GetU64(const std::string &key, uint64_t &u64);
  int32_t SetKV(const std::string &key, const std::string &value);
  int32_t GetKV(const std::string &key, std::string *value);

 public:
  leveldb::DB *db;

 private:
  int32_t apply_count_;
  uint32_t check_sum_;
  std::string all_values_;
};

//------------------TestSM---------------------------

StateMachineSPtr CreateSM(std::string &path);

//-----------------------------------------

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
  kTestState10,
  kTestStateEnd,
};

inline std::string TestState2Str(TestState state) {
  switch (state) {
    case kTestState0:
      return "kTestState0";
    case kTestState1:
      return "kTestState1";
    case kTestState2:
      return "kTestState2";
    case kTestState3:
      return "kTestState3";
    case kTestState4:
      return "kTestState4";
    case kTestState5:
      return "kTestState5";
    case kTestState6:
      return "kTestState6";
    case kTestState7:
      return "kTestState7";
    case kTestState8:
      return "kTestState8";
    case kTestState9:
      return "kTestState9";
    case kTestState10:
      return "kTestState10";
    case kTestStateEnd:
      return "kTestStateEnd";
    default:
      return "UnknowState";
  }
}

struct StateChange {
  TestState next;
  CondFunc func;
};

extern TestState current_state;
extern std::unordered_map<TestState, StateChange> rules;

}  // namespace vraft

#endif
