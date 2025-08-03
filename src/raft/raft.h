#ifndef VRAFT_RAFT_H_
#define VRAFT_RAFT_H_

#include <functional>
#include <vector>

#include "append_entries.h"
#include "append_entries_reply.h"
#include "checker.h"
#include "client_request.h"
#include "config.h"
#include "config_manager.h"
#include "index_manager.h"
#include "install_snapshot.h"
#include "install_snapshot_reply.h"
#include "nlohmann/json.hpp"
#include "peer_manager.h"
#include "ping.h"
#include "ping_reply.h"
#include "raft_addr.h"
#include "raft_log.h"
#include "request_vote.h"
#include "request_vote_reply.h"
#include "simple_random.h"
#include "snapshot_manager.h"
#include "solid_data.h"
#include "state_machine.h"
#include "timeout_now.h"
#include "timer.h"
#include "timer_manager.h"
#include "tracer.h"
#include "vote_manager.h"

namespace vraft {

enum State {
  STATE_FOLLOWER = 0,
  STATE_CANDIDATE,
  STATE_LEADER,
  STATE_STANDBY,
  STATE_ERROR,
};

const char *StateToStr(enum State state);
void Tick(Timer *timer);
void Elect(Timer *timer);
void RequestVoteRpc(Timer *timer);
void HeartBeat(Timer *timer);

class Raft final {
 public:
  Raft(const std::string &path, const RaftConfig &rc);
  ~Raft();
  Raft(const Raft &r) = delete;
  Raft &operator=(const Raft &r) = delete;

  // life cycle
  int32_t Start();
  int32_t Stop();
  void Init();

  // client request
  int32_t Propose(std::string value, Functor cb);
  int32_t LeaderTransfer(RaftAddr &dest);
  int32_t LeaderTransferFirstPeer();
  int32_t AddServer(const RaftAddr &addr);
  int32_t RemoveServer(const RaftAddr &addr);

  // on message
  int32_t OnPing(struct Ping &msg);
  int32_t OnPingReply(struct PingReply &msg);
  int32_t OnRequestVote(struct RequestVote &msg);
  int32_t OnRequestVoteReply(struct RequestVoteReply &msg);
  int32_t OnAppendEntries(struct AppendEntries &msg);
  int32_t OnAppendEntriesReply(struct AppendEntriesReply &msg);
  int32_t OnInstallSnapshot(struct InstallSnapshot &msg);
  int32_t OnInstallSnapshotReply(struct InstallSnapshotReply &msg);
  int32_t OnTimeoutNow(struct TimeoutNow &msg);
  int32_t OnClientRequest(struct ClientRequest &msg,
                          vraft::TcpConnectionSPtr conn);

  // send message
  int32_t SendPing(uint64_t dest, Tracer *tracer);
  int32_t SendRequestVote(uint64_t dest, Tracer *tracer);
  int32_t SendAppendEntries(uint64_t dest, Tracer *tracer);
  int32_t SendInstallSnapshot(uint64_t dest, Tracer *tracer);
  int32_t SendRequestVoteReply(RequestVoteReply &msg, Tracer *tracer);
  int32_t SendAppendEntriesReply(AppendEntriesReply &msg, Tracer *tracer);
  int32_t SendInstallSnapshotReply(InstallSnapshotReply &msg, Tracer *tracer);
  int32_t SendTimeoutNow(uint64_t dest, bool force, Tracer *tracer);

  // utils
  int16_t Id() { return config_mgr_.Current()->me.id(); }
  RaftAddr Me() { return config_mgr_.Current()->me; }
  RaftTerm Term();
  std::vector<RaftAddr> Peers() { return config_mgr_.Current()->peers; }
  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
  void Print(bool tiny, bool one_line);
  void DoRequestVote(Tracer *tracer);
  void DoPreVote(Tracer *tracer);

  // get set
  bool started() const;
  enum State state();
  void set_send(SendFunc func);
  void set_make_timer(MakeTimerFunc func);
  void set_assert_loop(Functor assert_loop);
  void set_tracer_cb(TracerCb tracer_cb);
  void set_create_sm(CreateSMFunc func);
  void set_reader_sm(CreateReaderFunc func);
  void set_writer_sm(CreateWriterFunc func);
  void set_enable_send_func(Functor func);
  void set_disable_send_func(Functor func);
  void set_enable_recv_func(Functor func);
  void set_disable_recv_func(Functor func);

  int32_t leader_times() const;
  bool print_screen() const;
  void set_print_screen(bool print_screen);
  bool enable_pre_vote() const;
  void set_enable_pre_vote(bool enable_pre_vote);
  bool leader_transfer() const;
  void set_leader_transfer(bool leader_transfer);
  bool pre_voting() const;
  void set_pre_voting(bool pre_voting);
  bool interval_check() const;
  void set_interval_check(bool interval_check);
  StateMachineSPtr sm();
  RaftLog &log();
  SolidData &meta();
  bool standby() const;
  void set_standby(bool b);

  // debug
  void DisableSend();
  void EnableSend();
  void DisableRecv();
  void EnableRecv();
  int32_t StartElection();  // for debug!!!

 private:
  bool IfSelfVote();
  int32_t InitConfig();
  void UpdateConfigFile();

  void AppendNoop(Tracer *tracer);
  void MaybeCommit(Tracer *tracer);
  void BecomeLeader(Tracer *tracer);
  void BecomeCandidate(Tracer *tracer);
  void StateMachineApply(Tracer *tracer);
  void StepDown(RaftTerm new_term, Tracer *tracer);

  RaftIndex LastIndex();
  RaftTerm LastTerm();
  RaftTerm GetTerm(RaftIndex index);

  void ResetManagerPeers(const std::vector<RaftAddr> &peers);
  void ConfigChange(const RaftConfig &rc, RaftIndex i);
  void ConfigRollback(RaftIndex i);
  RaftConfig DoConfigChange(const RaftConfig &rc);
  void AddPeer(const RaftAddr &addr);
  void DeletePeer(const RaftAddr &addr);

  void AgainElection() { timer_mgr_.AgainElection(); }

  int32_t DoPropose(const std::string &value, EntryType type, Tracer *tracer);

 private:
  bool started_;
  std::string home_path_;
  std::string conf_path_;
  std::string meta_path_;
  std::string log_path_;
  std::string sm_path_;

  int32_t seqid_;

  // raft state: every server
  enum State state_;
  RaftIndex commit_;
  RaftIndex last_apply_;

  RaftLog log_;
  SolidData meta_;

  RaftAddr leader_;  // leader cache
  ConfigManager config_mgr_;

  // raft state: candidate
  VoteManager vote_mgr_;

  // raft state: leader
  IndexManager index_mgr_;

  // raft state: state machine
  StateMachineSPtr sm_;

  // raft state: snapshot
  SnapshotManager snapshot_mgr_;

  // helper
  TimerManager timer_mgr_;
  SendFunc send_;
  MakeTimerFunc make_timer_;
  Functor assert_loop_;
  TracerCb tracer_cb_;
  CreateSMFunc create_sm_;
  CreateReaderFunc create_reader_;
  CreateWriterFunc create_writer_;
  Functor enable_send_func_;
  Functor disable_send_func_;
  Functor enable_recv_func_;
  Functor disable_recv_func_;
  bool enable_send_;
  bool enable_recv_;
  int32_t leader_times_;

  bool print_screen_;
  bool enable_pre_vote_;
  bool pre_voting_;
  bool leader_transfer_;
  RaftTerm transfer_max_term_;
  bool interval_check_;
  int64_t last_heartbeat_timestamp_;
  RaftIndex changing_index_;  // config changing index

  // when add into a cluster
  // when standby_, cannot send out request-vote
  // when config change finish, standby_ is clear
  bool standby_;

  friend void Tick(Timer *timer);
  friend void Elect(Timer *timer);
  friend void RequestVoteRpc(Timer *timer);
  friend void HeartBeat(Timer *timer);
};

inline bool Raft::started() const { return started_; }

inline enum State Raft::state() { return state_; }

inline void Raft::set_send(SendFunc func) { send_ = func; }

inline void Raft::set_make_timer(MakeTimerFunc func) { make_timer_ = func; }

inline void Raft::set_assert_loop(Functor assert_loop) {
  assert_loop_ = assert_loop;
}

inline void Raft::set_tracer_cb(TracerCb tracer_cb) { tracer_cb_ = tracer_cb; }

inline void Raft::set_create_sm(CreateSMFunc func) { create_sm_ = func; }

inline void Raft::set_reader_sm(CreateReaderFunc func) {
  create_reader_ = func;
}

inline void Raft::set_writer_sm(CreateWriterFunc func) {
  create_writer_ = func;
}

inline void Raft::set_enable_send_func(Functor func) {
  enable_send_func_ = func;
}

inline void Raft::set_disable_send_func(Functor func) {
  disable_send_func_ = func;
}

inline void Raft::set_enable_recv_func(Functor func) {
  enable_recv_func_ = func;
}

inline void Raft::set_disable_recv_func(Functor func) {
  disable_recv_func_ = func;
}

inline int32_t Raft::leader_times() const { return leader_times_; }

inline bool Raft::print_screen() const { return print_screen_; }

inline void Raft::set_print_screen(bool print_screen) {
  print_screen_ = print_screen;
}

inline bool Raft::enable_pre_vote() const { return enable_pre_vote_; }

inline void Raft::set_enable_pre_vote(bool enable_pre_vote) {
  enable_pre_vote_ = enable_pre_vote;
}

inline bool Raft::leader_transfer() const { return leader_transfer_; }

inline void Raft::set_leader_transfer(bool leader_transfer) {
  leader_transfer_ = leader_transfer;
}

inline bool Raft::pre_voting() const { return pre_voting_; }

inline void Raft::set_pre_voting(bool pre_voting) { pre_voting_ = pre_voting; }

inline bool Raft::interval_check() const { return interval_check_; }

inline void Raft::set_interval_check(bool interval_check) {
  interval_check_ = interval_check;
}

inline StateMachineSPtr Raft::sm() { return sm_; }

inline RaftLog &Raft::log() { return log_; }

inline SolidData &Raft::meta() { return meta_; }

inline bool Raft::standby() const { return standby_; }

inline void Raft::set_standby(bool b) { standby_ = b; };

}  // namespace vraft

#endif
