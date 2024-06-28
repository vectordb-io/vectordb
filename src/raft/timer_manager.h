#ifndef VRAFT_TIMER_MANAGER_H_
#define VRAFT_TIMER_MANAGER_H_

#include <unordered_map>

#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "simple_random.h"
#include "timer.h"
#include "util.h"

namespace vraft {

class TimerManager final {
 public:
  TimerManager(const std::vector<RaftAddr> &peers);
  ~TimerManager();
  TimerManager(const TimerManager &t) = delete;
  TimerManager &operator=(const TimerManager &t) = delete;

  void Reset(const std::vector<RaftAddr> &peers);
  void MakeTimer();
  void MakeTick();
  void MakeElection();
  void MakeElectionPpc();
  void MakeHeartbeat();

  void StartTick();
  void StartElection();
  void AgainElection();
  void StartRequestVote();
  void StartRequestVote(uint64_t addr);
  void StartHeartBeat();
  void StartHeartBeat(uint64_t addr);
  void AgainHeartBeat();
  void AgainHeartBeat(uint64_t addr);

  void Stop();
  void StopTick();
  void StopElection();
  void StopRequestVote();
  void StopRequestVote(uint64_t addr);
  void StopHeartBeat();
  void StopHeartBeat(uint64_t addr);

  void Close();
  void CloseTick();
  void CloseElection();
  void CloseRequestVote();
  void CloseRequestVote(uint64_t addr);
  void CloseHeartBeat();
  void CloseHeartBeat(uint64_t addr);

  void set_data(void *data);
  void set_tick_func(TimerFunctor func);
  void set_election_func(TimerFunctor func);
  void set_requestvote_func(TimerFunctor func);
  void set_heartbeat_func(TimerFunctor func);
  void set_maketimer_func(MakeTimerFunc func);

  uint32_t last_election_ms() const;
  uint32_t next_election_ms() const;

 private:
  void *data_;
  TimerSPtr tick_;
  TimerSPtr election_;
  std::unordered_map<uint64_t, TimerSPtr> request_votes_;
  std::unordered_map<uint64_t, TimerSPtr> heartbeats_;

  uint32_t tick_ms_;
  uint32_t election_ms_;
  uint32_t request_vote_ms_;
  uint32_t heartbeat_ms_;
  SimpleRandom random_election_ms_;

  uint32_t last_election_ms_;
  uint32_t next_election_ms_;

  TimerFunctor tick_func_;
  TimerFunctor election_func_;
  TimerFunctor requestvote_func_;
  TimerFunctor heartbeat_func_;
  MakeTimerFunc maketimer_func_;
};

inline TimerManager::TimerManager(const std::vector<RaftAddr> &peers)
    : data_(nullptr),
      tick_ms_(1000),
      election_ms_(2000),
      request_vote_ms_(1000),
      heartbeat_ms_(500),
      random_election_ms_(election_ms_, 2 * election_ms_),
      last_election_ms_(0),
      next_election_ms_(0) {
  Reset(peers);
}

inline TimerManager::~TimerManager() {}

inline void TimerManager::Reset(const std::vector<RaftAddr> &peers) {
  request_votes_.clear();
  heartbeats_.clear();
  for (auto addr : peers) {
    request_votes_[addr.ToU64()] = nullptr;
    heartbeats_[addr.ToU64()] = nullptr;
  }
}

inline void TimerManager::set_data(void *data) { data_ = data; }

inline void TimerManager::set_tick_func(TimerFunctor func) {
  tick_func_ = func;
}

inline void TimerManager::set_election_func(TimerFunctor func) {
  election_func_ = func;
}

inline void TimerManager::set_requestvote_func(TimerFunctor func) {
  requestvote_func_ = func;
}

inline void TimerManager::set_heartbeat_func(TimerFunctor func) {
  heartbeat_func_ = func;
}

inline void TimerManager::set_maketimer_func(MakeTimerFunc func) {
  maketimer_func_ = func;
}

inline uint32_t TimerManager::last_election_ms() const {
  return last_election_ms_;
}

inline uint32_t TimerManager::next_election_ms() const {
  return next_election_ms_;
}

}  // namespace vraft

#endif
