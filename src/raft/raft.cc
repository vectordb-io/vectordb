#include "raft.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "clock.h"
#include "raft_server.h"
#include "util.h"
#include "vraft_logger.h"

namespace vraft {

const char *StateToStr(enum State state) {
  switch (state) {
    case FOLLOWER:
      return "FOLLOWER";
      break;
    case CANDIDATE:
      return "CANDIDATE";
      break;
    case LEADER:
      return "LEADER";
      break;
    default:
      assert(0);
  }
}

void Tick(Timer *timer) {
  Raft *r = reinterpret_cast<Raft *>(timer->data());
  vraft_logger.FInfo("raft-tick: %s", r->ToJsonString(true, true).c_str());
  for (auto &dest_addr : r->Peers()) {
    r->SendPing(dest_addr.ToU64(), nullptr);
  }
}

// if path is empty, use rc to initialize,
// else use the data in path to initialize
Raft::Raft(const std::string &path, const RaftConfig &rc)
    : started_(false),
      home_path_(path),
      conf_path_(path + "/conf/conf.json"),
      meta_path_(path + "/meta"),
      log_path_(path + "/rlog"),
      sm_path_(path + "/sm"),
      state_(FOLLOWER),
      commit_(0),
      last_apply_(0),
      log_(path + "/rlog"),
      meta_(path + "/meta"),
      leader_(0),
      config_mgr_(rc),
      vote_mgr_(rc.peers),
      index_mgr_(rc.peers),
      sm_(nullptr),
      timer_mgr_(rc.peers),
      send_(nullptr),
      make_timer_(nullptr) {
  vraft_logger.FInfo("raft construct, %s, %p", rc.me.ToString().c_str(), this);
}

Raft::~Raft() { vraft_logger.FInfo("raft destruct, %p", this); }

int32_t Raft::Start() {
  if (assert_loop_) {
    assert_loop_();
  }

  Tracer tracer(this, true, tracer_cb_);
  tracer.PrepareState0();
  tracer.PrepareEvent(kEventStart, "raft start");

  // create sm
  if (create_sm_) {
    sm_ = create_sm_(sm_path_);
    assert(sm_);
  }

  // state machine restore
  if (sm_) {
    sm_->Restore();
    last_apply_ = sm_->LastIndex();
  }

  started_ = true;

  // make timer
  assert(make_timer_);
  timer_mgr_.set_maketimer_func(make_timer_);
  timer_mgr_.set_tick_func(Tick);
  timer_mgr_.set_election_func(Elect);
  timer_mgr_.set_requestvote_func(RequestVoteRpc);
  timer_mgr_.set_heartbeat_func(HeartBeat);
  timer_mgr_.set_data(this);
  timer_mgr_.MakeTimer();

  // start tick
  timer_mgr_.StartTick();

  // become follower
  StepDown(meta_.term(), &tracer);

  tracer.PrepareState1();
  tracer.Finish();
  return 0;
}

int32_t Raft::Stop() {
  if (assert_loop_) {
    assert_loop_();
  }

  Tracer tracer(this, true, tracer_cb_);
  tracer.PrepareState0();
  tracer.PrepareEvent(kEventStop, "raft stop");

  started_ = false;
  timer_mgr_.Close();

  tracer.PrepareState1();
  tracer.Finish();
  return 0;
}

void Raft::Init() {
  if (assert_loop_) {
    assert_loop_();
  }

  int32_t rv;
  char cmd_buf[256];

  snprintf(cmd_buf, sizeof(cmd_buf), "mkdir -p %s/conf", home_path_.c_str());
  rv = system(cmd_buf);
  assert(rv == 0);

  snprintf(cmd_buf, sizeof(cmd_buf), "mkdir -p %s", log_path_.c_str());
  system(cmd_buf);
  assert(rv == 0);

  snprintf(cmd_buf, sizeof(cmd_buf), "mkdir -p %s", meta_path_.c_str());
  system(cmd_buf);
  assert(rv == 0);

  snprintf(cmd_buf, sizeof(cmd_buf), "mkdir -p %s", sm_path_.c_str());
  system(cmd_buf);
  assert(rv == 0);

  rv = InitConfig();
  assert(rv == 0);

  meta_.Init();
  log_.Init();

  // reset managers
  index_mgr_.ResetNext(LastIndex() + 1);
  index_mgr_.ResetMatch(0);
}

int32_t Raft::InitConfig() {
  std::ifstream read_file(conf_path_);
  if (read_file) {
    read_file.close();

    // load config
    std::ifstream json_file(conf_path_);
    nlohmann::json j;
    json_file >> j;
    json_file.close();

    RaftConfig rc;
    rc.me = RaftAddr(j["config_manager"]["me"][0]);
    for (auto &peer : j["config_manager"]["peers"]) {
      rc.peers.push_back(RaftAddr(peer[0]));
    }
    config_mgr_.SetCurrent(rc);

    // reset managers
    index_mgr_.Reset(rc.peers);
    vote_mgr_.Reset(rc.peers);
    timer_mgr_.Reset(rc.peers);

  } else {
    // write config
    std::ofstream write_file(conf_path_);
    write_file << config_mgr_.ToJsonString(false, false);
    write_file.close();
  }

  return 0;
}

RaftIndex Raft::LastIndex() {
  RaftIndex snapshot_last = 0;
  if (sm_) {
    snapshot_last = sm_->LastIndex();
  }

  RaftIndex log_last = log_.Last();
  RaftIndex last = std::max(snapshot_last, log_last);
  return last;
}

RaftTerm Raft::GetTerm(RaftIndex index) {
  assert(index >= 0);
  if (index == 0) {
    return 0;
  }

  assert(index >= 1);
  MetaValue meta;
  int32_t rv = log_.GetMeta(index, meta);
  if (rv == 0) {
    return meta.term;
  } else {
    assert(sm_);
    return sm_->LastTerm();
  }
}

bool Raft::IfSelfVote() { return (meta_.vote() == Me().ToU64()); }

void Raft::AppendNoop(Tracer *tracer) {
  AppendEntry entry;
  entry.term = meta_.term();
  entry.type = kNoop;
  entry.value.append("0");
  int32_t rv = log_.AppendOne(entry, tracer);
  assert(rv == 0);
}

int32_t Raft::SendPing(uint64_t dest, Tracer *tracer) {
  Ping msg;
  msg.src = Me();
  msg.dest = RaftAddr(dest);
  msg.uid = UniqId(&msg);
  msg.msg = "ping";

  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kPing;
  std::string header_str;
  header.ToString(header_str);

  if (send_) {
    header_str.append(std::move(body_str));
    send_(dest, header_str.data(), header_str.size());

    if (tracer != nullptr) {
      tracer->PrepareEvent(kEventSend, msg.ToJsonString(false, true));
    }
  }
  return 0;
}

nlohmann::json Raft::ToJson() {
  nlohmann::json j;
  j["index"] = index_mgr_.ToJson();
  j["vote"] = vote_mgr_.ToJson();
  j["config"] = config_mgr_.ToJson();
  j["log"] = log_.ToJson();
  j["meta"] = meta_.ToJson();
  j["commit"] = commit_;
  j["last_apply"] = last_apply_;
  j["state"] = std::string(StateToStr(state_));
  j["run"] = started_;
  if (leader_.ToU64() == 0) {
    j["leader"] = 0;
  } else {
    j["leader"] = leader_.ToString();
  }
  j["this"] = PointerToHexStr(this);
  return j;
}

nlohmann::json Raft::ToJsonTiny() {
  nlohmann::json j;

  j[0][0]["term"] = meta_.term();
  if (meta_.vote() == 0) {
    j[0][0]["vote"] = "0";
  } else {
    RaftAddr addr(meta_.vote());
    j[0][0]["vote"] = addr.ToString();
  }
  j[0][1]["log"] = log_.ToJsonTiny();

  j[0][2]["apply"] = last_apply_;
  j[0][2]["cmt"] = commit_;
  j[0][2]["leader"] = leader_.ToString();
  j[0][2]["run"] = started_;
  j[0][2]["elect_ms"][0] = timer_mgr_.last_election_ms();
  j[0][2]["elect_ms"][1] = timer_mgr_.next_election_ms();

  for (auto dest : config_mgr_.Current().peers) {
    std::string key;
    key.append(dest.ToString());
    j[0][3][key][0]["match"] = index_mgr_.indices[dest.ToU64()].match;
    j[0][3][key][0]["next"] = index_mgr_.indices[dest.ToU64()].next;
    j[0][3][key][1]["grant"] = vote_mgr_.votes[dest.ToU64()].grant;
    j[0][3][key][1]["done"] = vote_mgr_.votes[dest.ToU64()].done;
  }

  j[1] = PointerToHexStr(this);
  return j;
}

std::string Raft::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j[config_mgr_.Current().me.ToString()][0] = std::string(StateToStr(state_));
    j[config_mgr_.Current().me.ToString()][1] = ToJsonTiny();
  } else {
    j[config_mgr_.Current().me.ToString()][0] = std::string(StateToStr(state_));
    j[config_mgr_.Current().me.ToString()][1] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

void Raft::Print(bool tiny, bool one_line) {
  printf("%s\n", ToJsonString(tiny, one_line).c_str());
  fflush(nullptr);
}

}  // namespace vraft
