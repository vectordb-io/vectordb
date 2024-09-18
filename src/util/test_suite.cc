#include "test_suite.h"

#include <gtest/gtest.h>

#include <csignal>

#include "coding.h"
#include "common.h"
#include "eventloop.h"
#include "raft.h"
#include "remu.h"
#include "timer.h"
#include "util.h"

namespace vraft {

EventLoopSPtr gtest_loop;
RemuSPtr gtest_remu;
std::string gtest_path;

uint16_t normal_port = 8000;
uint16_t standby_port = 9000;

bool gtest_enable_pre_vote;
bool gtest_interval_check;
bool gtest_desc;
int32_t gtest_node_num = 3;

void RemuParseConfig(int argc, char **argv) {
  gtest_enable_pre_vote = false;
  gtest_interval_check = false;
  gtest_desc = false;

  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == std::string("--enable-pre-vote")) {
      gtest_enable_pre_vote = true;
    }

    if (std::string(argv[i]) == std::string("--enable-interval-check")) {
      gtest_interval_check = true;
    }

    if (std::string(argv[i]) == std::string("--desc")) {
      gtest_desc = true;
    }

    {
      std::string arg_str = std::string(std::string(argv[i]));
      std::string key_str = std::string("--node-num=");
      if (arg_str.compare(0, key_str.size(), key_str) == 0) {
        arg_str.erase(0, key_str.size());
        sscanf(arg_str.c_str(), "%d", &gtest_node_num);
      }
    }
  }
}

void RemuLogState(std::string key) {
  if (gtest_remu) {
    gtest_remu->Log(key);

    // check when state change
    gtest_remu->Check();
  }
}

void PrintAndCheck() {
  printf("--- %s --- all leader times:%d \n",
         TestState2Str(current_state).c_str(), gtest_remu->LeaderTimes());
  gtest_remu->Print();
  gtest_remu->Check();
}

void GenerateConfig(std::vector<Config> &configs, int32_t peers_num) {
  configs.clear();
  GetConfig().peers().clear();
  GetConfig().set_my_addr(HostPort("127.0.0.1", normal_port));
  for (int i = 1; i <= peers_num; ++i) {
    GetConfig().peers().push_back(HostPort("127.0.0.1", normal_port + i));
  }
  GetConfig().set_log_level(kLoggerTrace);
  GetConfig().set_enable_debug(true);
  GetConfig().set_path(gtest_path);
  GetConfig().set_mode(kSingleMode);

  GenerateRotateConfig(configs);
  std::cout << "generate configs, size:" << configs.size() << std::endl;
}

void GTestSignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << std::endl;
  std::cout << "exit ..." << std::endl;
  gtest_loop->RunFunctor(std::bind(&Remu::Stop, gtest_remu.get()));
  gtest_loop->Stop();
}

StateMachineSPtr CreateSM(std::string &path) {
  StateMachineSPtr sptr(new TestSM(path));
  return sptr;
}

void RemuTestSetUp(std::string path, GTestTickFunc tick_func,
                   CreateSMFunc create_sm) {
  std::cout << "setting up test... \n";
  std::fflush(nullptr);
  gtest_path = path;
  std::string cmd = "rm -rf " + gtest_path;
  system(cmd.c_str());

  LoggerOptions logger_options{"vraft", false, 1, 8192, kLoggerTrace, true};
  std::string log_file = gtest_path + "/log/remu.log";
  vraft_logger.Init(log_file, logger_options);

  std::signal(SIGINT, GTestSignalHandler);
  CodingInit();

  assert(!gtest_loop);
  assert(!gtest_remu);
  gtest_loop = std::make_shared<EventLoop>("remu-loop");
  int32_t rv = gtest_loop->Init();
  ASSERT_EQ(rv, 0);

  gtest_remu = std::make_shared<Remu>(gtest_loop, gtest_enable_pre_vote,
                                      gtest_interval_check);
  gtest_remu->tracer_cb = RemuLogState;
  gtest_remu->create_sm = create_sm;

  TimerParam param;
  param.timeout_ms = 0;
  param.repeat_ms = 1000;
  param.cb = tick_func;
  param.data = nullptr;
  param.name = "remu-timer";
  param.repeat_times = 5;
  gtest_loop->AddTimer(param);

  // important !!
  current_state = kTestState0;
}

void RemuTestTearDown() {
  std::cout << "tearing down test... \n";
  std::fflush(nullptr);

  gtest_remu->Clear();
  gtest_remu.reset();
  gtest_loop.reset();
  Logger::ShutDown();

  // system("rm -rf /tmp/remu_test_dir");
}

void RunRemuTest(int32_t node_num) {
  int32_t peers_num = node_num - 1;
  GenerateConfig(gtest_remu->configs, peers_num);
  gtest_remu->Create();
  gtest_remu->Start();

  {
    EventLoopSPtr l = gtest_loop;
    std::thread t([l]() { l->Loop(); });
    l->WaitStarted();
    t.join();
  }

  std::cout << "join thread... \n";
  std::fflush(nullptr);
}

void RunRemuTest2(int32_t node_num, int32_t add_node_num) {
  int32_t peers_num = node_num - 1;
  GenerateConfig(gtest_remu->configs, peers_num);
  for (int32_t i = 0; i < add_node_num; ++i) {
    gtest_remu->AddOneNode();
  }

  gtest_remu->PrintConfig();

  gtest_remu->Create();
  gtest_remu->Start();

  {
    EventLoopSPtr l = gtest_loop;
    std::thread t([l]() { l->Loop(); });
    l->WaitStarted();
    t.join();
  }

  std::cout << "join thread... \n";
  std::fflush(nullptr);
}

//------------------TestSM---------------------------

const std::string last_index_key = "LAST_INDEX_KEY";
const std::string last_term_key = "LAST_TERM_KEY";

const std::string all_values_key = "ALL_VALUES_KEY";
const std::string apply_count_key = "APPLY_COUNT_KEY";
const std::string check_sum_key = "CHECK_SUM_KEY";

TestSM::TestSM(std::string path)
    : StateMachine(path), db(nullptr), apply_count_(0), check_sum_(0) {
  leveldb::Options o;
  o.create_if_missing = true;
  o.error_if_exists = false;
  leveldb::Status status = leveldb::DB::Open(o, path, &db);
  assert(status.ok());

  int32_t rv = GetI32(apply_count_key, apply_count_);
  if (rv != 1) {
    apply_count_ = 0;
    rv = SetU32(apply_count_key, apply_count_);
    assert(rv == 0);
  }

  rv = GetU32(check_sum_key, check_sum_);
  if (rv != 1) {
    check_sum_ = 0;
    rv = SetU32(check_sum_key, check_sum_);
    assert(rv == 0);
  }

  rv = GetKV(all_values_key, &all_values_);
  if (rv != 1) {
    all_values_ = "";
    rv = SetKV(all_values_key, all_values_);
    assert(rv == 0);
  }
}

TestSM::~TestSM() { delete db; }

int32_t TestSM::Restore() {
  printf("\n\n****** TestSM Restore ****** path:%s \n\n", path().c_str());
  fflush(nullptr);
  return 0;
}

int32_t TestSM::Get(const std::string &key, std::string &value) {
  return GetKV(key, &value);
}

// format: key:value
int32_t TestSM::Apply(LogEntry *entry, RaftAddr addr) {
  printf("\n\n****** TestSM Apply %s ****** entry:%s \n\n",
         addr.ToString().c_str(), entry->ToJsonString(true, true).c_str());
  fflush(nullptr);

  leveldb::WriteBatch batch;

  // last index
  {
    char buf[sizeof(uint32_t)];
    EncodeFixed32(buf, entry->index);
    batch.Put(leveldb::Slice(last_index_key),
              leveldb::Slice(buf, sizeof(uint32_t)));
  }

  // last term
  {
    char buf[sizeof(uint64_t)];
    EncodeFixed64(buf, entry->append_entry.term);
    batch.Put(leveldb::Slice(last_term_key),
              leveldb::Slice(buf, sizeof(uint64_t)));
  }

  // key:value
  {
    std::vector<std::string> kv;
    Split(entry->append_entry.value, ':', kv);
    assert(kv.size() == 2);
    batch.Put(leveldb::Slice(kv[0]), leveldb::Slice(kv[1]));
  }

  // apply_count_
  {
    ++apply_count_;
    char buf[sizeof(uint32_t)];
    EncodeFixed32(buf, apply_count_);
    batch.Put(leveldb::Slice(apply_count_key),
              leveldb::Slice(buf, sizeof(uint32_t)));
  }

  // check_sum_
  {
    uint32_t new_check_sum = Crc32(entry->append_entry.value.c_str(),
                                   entry->append_entry.value.size());
    check_sum_ = check_sum_ ^ new_check_sum;

    char buf[sizeof(uint32_t)];
    EncodeFixed32(buf, check_sum_);
    batch.Put(leveldb::Slice(check_sum_key),
              leveldb::Slice(buf, sizeof(uint32_t)));
  }

  // all_values_
  {
    all_values_ = all_values_ + " + " + entry->append_entry.value;
    batch.Put(leveldb::Slice(all_values_key), leveldb::Slice(all_values_));
  }

  leveldb::WriteOptions wo;
  wo.sync = true;
  leveldb::Status s = db->Write(wo, &batch);
  assert(s.ok());

  return 0;
}

RaftIndex TestSM::LastIndex() {
  RaftIndex index;
  int32_t rv = GetU32(last_index_key, index);
  if (rv == 1) {
    return index;
  } else {
    return 0;
  }
}

RaftTerm TestSM::LastTerm() {
  RaftTerm term;
  int32_t rv = GetU64(last_term_key, term);
  if (rv == 1) {
    return term;
  } else {
    return 0;
  }
}

int32_t TestSM::SetApplyCount(int32_t apply_count) {
  return SetI32(apply_count_key, apply_count);
}

int32_t TestSM::GetApplyCount(int32_t &apply_count) {
  return GetI32(apply_count_key, apply_count);
}

int32_t TestSM::SetCheckSum(int32_t check_sum) {
  return SetI32(check_sum_key, check_sum);
}

int32_t TestSM::GetCheckSum(int32_t &check_sum) {
  return GetI32(check_sum_key, check_sum);
}

int32_t TestSM::SetAllValues(const std::string &value) {
  return SetKV(all_values_key, value);
}

int32_t TestSM::GetAllValues(std::string *value) {
  return GetKV(all_values_key, value);
}

// return 0: ok
// return -1: error
int32_t TestSM::SetI32(const std::string &key, int32_t i32) {
  leveldb::WriteBatch batch;
  {
    char buf[sizeof(i32)];
    EncodeFixed32(buf, i32);
    batch.Put(leveldb::Slice(key), leveldb::Slice(buf, sizeof(i32)));
  }

  leveldb::WriteOptions wo;
  wo.sync = true;
  leveldb::Status s = db->Write(wo, &batch);
  assert(s.ok());

  return 0;
}

// return 0: not find
// return 1: find
// return -1: error
int32_t TestSM::GetI32(const std::string &key, int32_t &i32) {
  leveldb::ReadOptions ro;
  leveldb::Status s;
  std::string value;
  s = db->Get(ro, leveldb::Slice(key), &value);
  if (s.ok()) {
    assert(value.size() == sizeof(i32));
    i32 = DecodeFixed32(value.c_str());
    return 1;
  } else if (s.IsNotFound()) {
    return 0;
  } else {
    return -1;
  }
}

int32_t TestSM::SetU32(const std::string &key, uint32_t u32) {
  leveldb::WriteBatch batch;
  {
    char buf[sizeof(u32)];
    EncodeFixed32(buf, u32);
    batch.Put(leveldb::Slice(key), leveldb::Slice(buf, sizeof(u32)));
  }

  leveldb::WriteOptions wo;
  wo.sync = true;
  leveldb::Status s = db->Write(wo, &batch);
  assert(s.ok());

  return 0;
}

int32_t TestSM::GetU32(const std::string &key, uint32_t &u32) {
  leveldb::ReadOptions ro;
  leveldb::Status s;
  std::string value;
  s = db->Get(ro, leveldb::Slice(key), &value);
  if (s.ok()) {
    assert(value.size() == sizeof(u32));
    u32 = DecodeFixed32(value.c_str());
    return 1;
  } else if (s.IsNotFound()) {
    return 0;
  } else {
    return -1;
  }
}

int32_t TestSM::SetU64(const std::string &key, uint64_t u64) {
  leveldb::WriteBatch batch;
  {
    char buf[sizeof(u64)];
    EncodeFixed64(buf, u64);
    batch.Put(leveldb::Slice(key), leveldb::Slice(buf, sizeof(u64)));
  }

  leveldb::WriteOptions wo;
  wo.sync = true;
  leveldb::Status s = db->Write(wo, &batch);
  assert(s.ok());

  return 0;
}

int32_t TestSM::GetU64(const std::string &key, uint64_t &u64) {
  leveldb::ReadOptions ro;
  leveldb::Status s;
  std::string value;
  s = db->Get(ro, leveldb::Slice(key), &value);
  if (s.ok()) {
    assert(value.size() == sizeof(u64));
    u64 = DecodeFixed64(value.c_str());
    return 1;
  } else if (s.IsNotFound()) {
    return 0;
  } else {
    return -1;
  }
}

int32_t TestSM::SetKV(const std::string &key, const std::string &value) {
  leveldb::WriteBatch batch;
  batch.Put(leveldb::Slice(key), leveldb::Slice(value));

  leveldb::WriteOptions wo;
  wo.sync = true;
  leveldb::Status s = db->Write(wo, &batch);
  assert(s.ok());

  return 0;
}

int32_t TestSM::GetKV(const std::string &key, std::string *value) {
  value->clear();
  leveldb::ReadOptions ro;
  leveldb::Status s;
  s = db->Get(ro, leveldb::Slice(key), value);
  if (s.ok()) {
    return 1;
  } else if (s.IsNotFound()) {
    return 0;
  } else {
    return -1;
  }
}

nlohmann::json TestSM::ToJson() {
  nlohmann::json j;
  j["db"] = PointerToHexStr(db);
  j["apply_count"] = apply_count_;
  j["check_sum"] = U32ToHexStr(check_sum_);
  j["all_values"] = all_values_;
  j["last_index"] = LastIndex();
  j["last_term"] = LastTerm();
  return j;
}

nlohmann::json TestSM::ToJsonTiny() {
  nlohmann::json j;
  j["apl-cnt"] = apply_count_;
  j["chk"] = U32ToHexStr(check_sum_);
  j["lst"] = LastIndex();
  j["ltm"] = LastTerm();
  return j;
}

std::string TestSM::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;

  if (tiny) {
    j["TestSM"] = ToJsonTiny();
  } else {
    j["TestSM"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

//------------------TestSM---------------------------

//-----------------------------------------

TimerFunctor timer_func;
TestState current_state = kTestState0;
std::unordered_map<TestState, StateChange> rules;

bool HasLeader() { return true; }

void InitRemuTest() {
  rules[kTestState0].next = kTestStateEnd;
  rules[kTestState0].func = HasLeader;
}

}  // namespace vraft
