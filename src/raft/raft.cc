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
    case STATE_FOLLOWER:
      return "FOLLOWER";
    case STATE_CANDIDATE:
      return "CANDIDATE";
    case STATE_LEADER:
      return "LEADER";
    case STATE_STANDBY:
      return "STANDBY";
    default:
      return "STATE-ERROR";
  }
}

void Tick(Timer *timer) {
  Raft *r = reinterpret_cast<Raft *>(timer->data());
  vraft_logger.FInfo("raft-tick: %s", r->ToJsonString(true, true).c_str());
  for (auto &dest_addr : r->Peers()) {
    //   r->SendPing(dest_addr.ToU64(), nullptr);
  }

  if (r->print_screen()) {
    printf("%s %s\n", NsToString(Clock::NSec()).c_str(),
           r->ToJsonString(true, true).c_str());
    fflush(nullptr);
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
      state_(STATE_FOLLOWER),
      commit_(0),
      last_apply_(0),
      log_(path + "/rlog"),
      meta_(path + "/meta"),
      leader_(0),
      config_mgr_(rc),
      vote_mgr_(rc.peers),
      index_mgr_(rc.peers),
      sm_(nullptr),
      snapshot_mgr_(rc.peers),
      timer_mgr_(rc.peers),
      send_(nullptr),
      make_timer_(nullptr),
      tracer_cb_(nullptr),
      create_sm_(nullptr),
      create_reader_(nullptr),
      create_writer_(nullptr),
      enable_send_func_(nullptr),
      disable_send_func_(nullptr),
      enable_recv_func_(nullptr),
      disable_recv_func_(nullptr),
      enable_send_(true),
      enable_recv_(true),
      leader_times_(0),
      print_screen_(false),
      enable_pre_vote_(false),
      pre_voting_(false),
      leader_transfer_(false),
      transfer_max_term_(0),
      interval_check_(true),
      last_heartbeat_timestamp_(0),
      changing_index_(0),
      standby_(false) {
  vraft_logger.FInfo("raft construct, %s, %p", rc.me.ToString().c_str(), this);
}

Raft::~Raft() { vraft_logger.FInfo("raft destruct, %p", this); }

int32_t Raft::Start() {
  if (assert_loop_) {
    assert_loop_();
  }

  Tracer tracer(this, true, tracer_cb_);
  tracer.PrepareState0();
  std::string str = Me().ToString() + std::string(" raft-start");
  tracer.PrepareEvent(kEventStart, str);

  // create sm
  if (create_sm_ && !sm_) {
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
  std::string str = Me().ToString() + std::string(" raft-stop");
  tracer.PrepareEvent(kEventStop, str);

  started_ = false;
  timer_mgr_.Stop();

  sm_.reset();

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

  meta_.Init();
  log_.Init();
  log_.me = Me();  // for debug
  log_.set_insert_cb(std::bind(&Raft::ConfigChange, this, std::placeholders::_1,
                               std::placeholders::_2));
  log_.set_delete_cb(
      std::bind(&Raft::ConfigRollback, this, std::placeholders::_1));

  rv = InitConfig();
  assert(rv == 0);

  // reset managers
  index_mgr_.ResetNext(LastIndex() + 1);
  index_mgr_.ResetMatch(0);
}

int32_t Raft::InitConfig() {
  Tracer tracer(this, true, tracer_cb_);
  tracer.PrepareState0();

  // set update-file cb
  config_mgr_.set_current_cb(std::bind(&Raft::UpdateConfigFile, this));

  RaftConfig rc;
  MetaValue mv;
  int32_t rv = log_.LastConfig(rc, mv);
  if (rv >= 0) {
    // use config in log, update current config
    config_mgr_.SetCurrent(rc);

  } else {
    // use config in param

    // append first config
    int32_t rv =
        log_.AppendFirstConfig(*(config_mgr_.Current()), meta_.term(), &tracer);
    assert(rv == 0);

    // update file
    config_mgr_.RunCb();
  }

  // reset managers
  ResetManagerPeers(config_mgr_.Current()->peers);

  {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s init-config %s", Me().ToString().c_str(),
             config_mgr_.Current()->ToJsonString(true, true).c_str());
    tracer.PrepareEvent(kEventOther, std::string(buf));
  }

  tracer.PrepareState1();
  tracer.Finish();
  return 0;
}

void Raft::UpdateConfigFile() {
  // write config
  std::ofstream write_file(conf_path_);
  write_file << config_mgr_.ToJsonString(false, false);
  write_file.close();
}

#if 0
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
    uint64_t u64 = j["config_manager"]["cur"]["me"][0];
    rc.me = RaftAddr(u64);
    for (auto &peer : j["config_manager"]["cur"]["peers"]) {
      u64 = peer[0];
      rc.peers.push_back(RaftAddr(u64));
    }
    config_mgr_.SetCurrent(rc);

    // reset managers
    index_mgr_.Reset(rc.peers);
    vote_mgr_.Reset(rc.peers);
    snapshot_mgr_.Reset(rc.peers);
    timer_mgr_.Reset(rc.peers);

  } else {
    // write config
    std::ofstream write_file(conf_path_);
    write_file << config_mgr_.ToJsonString(false, false);
    write_file.close();
  }

  return 0;
}
#endif

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

void Raft::ResetManagerPeers(const std::vector<RaftAddr> &peers) {
  // reset managers
  index_mgr_.Reset(peers);
  vote_mgr_.Reset(peers);
  snapshot_mgr_.Reset(peers);
  timer_mgr_.Reset(peers);
}

void Raft::ConfigChange(const RaftConfig &rc, RaftIndex i) {
  RaftConfig new_config = DoConfigChange(rc);
  config_mgr_.SetCurrent(new_config);
  changing_index_ = i;
}

RaftConfig Raft::DoConfigChange(const RaftConfig &rc) {
  std::vector<RaftAddr> diff_nodes;
  bool add = config_mgr_.Current()->IamIn(rc);
  bool rm = rc.IamIn(*(config_mgr_.Current()));

  RaftConfig new_config = *(config_mgr_.Current());
  if (!standby_) {
    assert((add && !rm) || (rm && !add));

    if (add && !rm) {  // add node
      int32_t rv = ConfigCompare(*(config_mgr_.Current()), rc, diff_nodes);
      assert(rv == -1);
      for (auto node : diff_nodes) {
        AddPeer(node);
        new_config.peers.push_back(node);
      }

    } else {  // rm node
      int32_t rv = ConfigCompare(*(config_mgr_.Current()), rc, diff_nodes);
      assert(rv == 1);
      for (auto node : diff_nodes) {
        DeletePeer(node);
        new_config.peers.erase(
            std::remove(new_config.peers.begin(), new_config.peers.end(), node),
            new_config.peers.end());
      }
    }

  } else {                 // standby_
    assert((add && !rm));  // add
    int32_t rv = ConfigCompare(*(config_mgr_.Current()), rc, diff_nodes);
    assert(rv == -1);
    for (auto node : diff_nodes) {
      AddPeer(node);
      new_config.peers.push_back(node);
    }
  }

  return new_config;
}

#if 0
void Raft::ConfigChange(const RaftConfig &rc, RaftIndex i) {
  std::vector<RaftAddr> add_nodes;

  bool in = config_mgr_.Current()->InConfig(rc.me);
  if (!in) {
    add_nodes.push_back(rc.me);
  }

  for (auto new_peer : rc.peers) {
    in = config_mgr_.Current()->InConfig(new_peer);
    if (!in) {
      add_nodes.push_back(new_peer);
    }
  }

  if (!standby_) {
    assert(add_nodes.size() == 1);
  }

  RaftConfig cur = *(config_mgr_.Current());
  for (auto add_addr : add_nodes) {
    cur.peers.push_back(add_addr);

    // add index-mgr
    {
      IndexItem item;
      item.next = 1;
      item.match = 0;

      if (state_ == STATE_LEADER) {
        item.next = LastIndex() + 1;
        item.match = 0;
      }

      index_mgr_.indices[add_addr.ToU64()] = item;
    }

    // add vote-mgr
    {
      VoteItem item;
      item.grant = false;
      item.done = false;
      item.logok = false;
      item.interval_ok = false;

      vote_mgr_.votes[add_addr.ToU64()] = item;
    }

    // add snapshot-mgr
    {
      Snapshot snapshot;
      snapshot.reader_ = nullptr;
      snapshot.writer_ = nullptr;
      snapshot_mgr_.snapshots[add_addr.ToU64()] = snapshot;
    }

    // add timer-mgr
    {
      // rpc timer request-vote
      {
        TimerSPtr sptr = timer_mgr_.CreateRpcTimer(add_addr);
        timer_mgr_.AddRpcTimer(add_addr, sptr);
      }

      // heartbeat timer
      {
        TimerSPtr sptr = timer_mgr_.CreateHeartbeatTimer(add_addr);
        timer_mgr_.AddHeartbeatTimer(add_addr, sptr);

        if (state_ == STATE_LEADER) {
          // sptr->Again(0, timer_mgr_.heartbeat_ms());
        }
      }
    }
  }

  config_mgr_.SetCurrent(cur);
  changing_index_ = i;
}
#endif

void Raft::ConfigRollback(RaftIndex i) {
  RaftConfig pre_config = *(config_mgr_.Previous());
  DoConfigChange(pre_config);
  config_mgr_.Rollback();

  if (changing_index_ == i) {
    changing_index_ = 0;
  }
}

void Raft::AddPeer(const RaftAddr &addr) {
  // add index-mgr
  {
    IndexItem item;
    item.next = 1;
    item.match = 0;

    if (state_ == STATE_LEADER) {
      item.next = LastIndex() + 1;
      item.match = 0;
    }

    index_mgr_.indices[addr.ToU64()] = item;
  }

  // add vote-mgr
  {
    VoteItem item;
    item.grant = false;
    item.done = false;
    item.logok = false;
    item.interval_ok = false;

    vote_mgr_.votes[addr.ToU64()] = item;
  }

  // add snapshot-mgr
  {
    Snapshot snapshot;
    snapshot.reader_ = nullptr;
    snapshot.writer_ = nullptr;
    snapshot_mgr_.snapshots[addr.ToU64()] = snapshot;
  }

  // add timer-mgr
  {
    // rpc timer request-vote
    {
      TimerSPtr sptr = timer_mgr_.CreateRpcTimer(addr);
      timer_mgr_.AddRpcTimer(addr, sptr);
    }

    // heartbeat timer
    {
      TimerSPtr sptr = timer_mgr_.CreateHeartbeatTimer(addr);
      timer_mgr_.AddHeartbeatTimer(addr, sptr);

      if (state_ == STATE_LEADER) {
        sptr->Again(0, timer_mgr_.heartbeat_ms());
      }
    }
  }
}

void Raft::DeletePeer(const RaftAddr &addr) {
  // delete index-mgr
  index_mgr_.indices.erase(addr.ToU64());

  // delete vote-mgr
  vote_mgr_.votes.erase(addr.ToU64());

  // delete snapshot-mgr
  snapshot_mgr_.snapshots.erase(addr.ToU64());

  // delete timer-mgr

  // rpc timer request-vote
  timer_mgr_.CloseRequestVote(addr.ToU64());
  timer_mgr_.DeleteRpcTimer(addr);

  // heartbeat timer
  timer_mgr_.CloseHeartBeat(addr.ToU64());
  timer_mgr_.DeleteHeartBeat(addr);
}

RaftTerm Raft::Term() { return meta_.term(); }

void Raft::DisableSend() {
  enable_send_ = false;

  if (disable_send_func_) {
    disable_send_func_();

    {
      Tracer tracer(this, true, tracer_cb_);
      tracer.PrepareState0();

      char buf[128];
      snprintf(buf, sizeof(buf), "%s disable-send", Me().ToString().c_str());
      tracer.PrepareEvent(kEventOther, std::string(buf));

      tracer.PrepareState1();
      tracer.Finish();
    }
  }
}

void Raft::EnableSend() {
  enable_send_ = true;

  if (enable_send_func_) {
    enable_send_func_();

    {
      Tracer tracer(this, true, tracer_cb_);
      tracer.PrepareState0();

      char buf[128];
      snprintf(buf, sizeof(buf), "%s enable-send", Me().ToString().c_str());
      tracer.PrepareEvent(kEventOther, std::string(buf));

      tracer.PrepareState1();
      tracer.Finish();
    }
  }
}

void Raft::DisableRecv() {
  enable_recv_ = false;

  if (disable_recv_func_) {
    disable_recv_func_();

    {
      Tracer tracer(this, true, tracer_cb_);
      tracer.PrepareState0();

      char buf[128];
      snprintf(buf, sizeof(buf), "%s disable-recv", Me().ToString().c_str());
      tracer.PrepareEvent(kEventOther, std::string(buf));

      tracer.PrepareState1();
      tracer.Finish();
    }
  }
}

void Raft::EnableRecv() {
  enable_recv_ = true;

  if (enable_recv_func_) {
    enable_recv_func_();

    {
      Tracer tracer(this, true, tracer_cb_);
      tracer.PrepareState0();

      char buf[128];
      snprintf(buf, sizeof(buf), "%s enable-recv", Me().ToString().c_str());
      tracer.PrepareEvent(kEventOther, std::string(buf));

      tracer.PrepareState1();
      tracer.Finish();
    }
  }
}

// for debug!!!
int32_t Raft::StartElection() {
  if (state_ == STATE_FOLLOWER) {
    timer_mgr_.StartElection();
    return 0;
  }
  return -1;
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
  msg.send_ts = Clock::NSec();
  msg.elapse = 0;
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
    int32_t rv = send_(dest, header_str.data(), header_str.size());

    if (tracer != nullptr && rv == 0) {
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

  if (sm_) {
    j["sm"] = sm_->ToJsonTiny();
  } else {
    j["sm"] = "null";
  }

  j["meta"] = meta_.ToJson();
  j["commit"] = commit_;
  j["last_apply"] = last_apply_;
  j["state"] = std::string(StateToStr(state_));
  j["leader_times"] = leader_times_;
  j["print"] = print_screen_;
  j["pre-vote"] = enable_pre_vote_;
  j["pre-voting"] = pre_voting_;
  j["transfer"] = leader_transfer_;
  j["tsf-max-term"] = transfer_max_term_;
  j["interval-chk"] = interval_check_;
  j["run"] = started_;
  if (leader_.ToU64() == 0) {
    j["leader"] = 0;
  } else {
    j["leader"] = leader_.ToString();
  }
  j["changing"] = changing_index_;
  j["standby"] = standby_;
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

  if (sm_) {
    j[0][1]["sm"] = sm_->ToJsonTiny();
  } else {
    j[0][1]["sm"] = "null";
  }

  j[0][2]["apply"] = last_apply_;
  j[0][2]["cmt"] = commit_;
  j[0][2]["leader"] = leader_.ToString();
  j[0][2]["run"] = started_;
  j[0][2]["elect-ms"][0] = timer_mgr_.last_election_ms();
  j[0][2]["elect-ms"][1] = timer_mgr_.next_election_ms();
  j[0][2]["leader_times"] = leader_times_;
  j[0][2]["print"] = print_screen_;
  j[0][2]["pre-vote"] = enable_pre_vote_;
  j[0][2]["pre-voting"] = pre_voting_;
  j[0][2]["transfer"] = leader_transfer_;
  j[0][2]["tsf-max-term"] = transfer_max_term_;
  j[0][2]["interval-chk"] = interval_check_;
  j[0][2]["last-hbts"] = NsToString2(last_heartbeat_timestamp_);
  j[0][2]["changing"] = changing_index_;
  j[0][2]["standby"] = standby_;

  for (auto dest : config_mgr_.Current()->peers) {
    std::string key;
    key.append(dest.ToString());
    // index_mgr_
    j[0][3][key][0]["match"] = index_mgr_.indices[dest.ToU64()].match;
    j[0][3][key][0]["next"] = index_mgr_.indices[dest.ToU64()].next;

    // vote_mgr_
    j[0][3][key][1]["grant"] = vote_mgr_.votes[dest.ToU64()].grant;
    j[0][3][key][1]["done"] = vote_mgr_.votes[dest.ToU64()].done;
    j[0][3][key][1]["logok"] = vote_mgr_.votes[dest.ToU64()].logok;
    j[0][3][key][1]["iok"] = vote_mgr_.votes[dest.ToU64()].interval_ok;
  }

  j[1] = PointerToHexStr(this);
  return j;
}

std::string Raft::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  std::string state_str = std::string(StateToStr(state_));
  if (!started_) {
    state_str += "(x)";
  } else {
    std::string tmp_str;

    if (enable_send_ && enable_recv_) {
      tmp_str = "";
    } else if (enable_send_ && !enable_recv_) {
      tmp_str = "(rx)";
    } else if (!enable_send_ && enable_recv_) {
      tmp_str = "(sx)";
    } else if (!enable_send_ && !enable_recv_) {
      tmp_str += "(sx-rx)";
    }

    state_str += tmp_str;
  }

  j[config_mgr_.Current()->me.ToString()][0] = state_str;
  if (tiny) {
    j[config_mgr_.Current()->me.ToString()][1] = ToJsonTiny();
  } else {
    j[config_mgr_.Current()->me.ToString()][1] = ToJson();
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
