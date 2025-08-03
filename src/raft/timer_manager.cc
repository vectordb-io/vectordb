#include "timer_manager.h"

namespace vraft {

void TimerManager::MakeTimer() {
  MakeTick();
  MakeElection();
  MakeElectionPpc();
  MakeHeartbeat();
}

void TimerManager::MakeTick() {
  assert(maketimer_func_);
  TimerParam param;
  param.timeout_ms = 0;
  param.repeat_ms = tick_ms_;
  param.cb = tick_func_;
  param.data = data_;
  param.name = "tick-timer";
  tick_ = maketimer_func_(param);
}

void TimerManager::MakeElection() {
  assert(maketimer_func_);
  TimerParam param;
  param.timeout_ms = random_election_ms_.Get();
  param.repeat_ms = 0;
  param.cb = election_func_;
  param.data = data_;
  param.name = "election-timer";
  election_ = maketimer_func_(param);
}

void TimerManager::AddRpcTimer(const RaftAddr &dest, TimerSPtr sptr) {
  auto it = request_votes_.find(dest.ToU64());
  assert(it == request_votes_.end());
  assert(dest.ToU64() == sptr->dest_addr());
  request_votes_[dest.ToU64()] = sptr;
}

void TimerManager::DeleteRpcTimer(const RaftAddr &dest) {
  request_votes_.erase(dest.ToU64());
}

void TimerManager::DeleteHeartBeat(const RaftAddr &dest) {
  heartbeats_.erase(dest.ToU64());
}

TimerSPtr TimerManager::CreateRpcTimer(const RaftAddr &dest) {
  assert(maketimer_func_);
  TimerSPtr sptr;
  TimerParam param;
  param.timeout_ms = 0;
  param.repeat_ms = request_vote_ms_;
  param.cb = requestvote_func_;
  param.data = data_;
  param.name = "rpc-timer";
  sptr = maketimer_func_(param);
  sptr->set_dest_addr(dest.ToU64());
  return sptr;
}

void TimerManager::MakeElectionPpc() {
  for (auto &item : request_votes_) {
    RaftAddr addr(item.first);
    TimerSPtr sptr = CreateRpcTimer(addr);
    item.second = sptr;
  }
}

#if 0
void TimerManager::MakeElectionPpc() {
  assert(maketimer_func_);
  for (auto &item : request_votes_) {
    TimerParam param;
    param.timeout_ms = 0;
    param.repeat_ms = request_vote_ms_;
    param.cb = requestvote_func_;
    param.data = data_;
    param.name = "rpc-timer";
    item.second = maketimer_func_(param);
    item.second->set_dest_addr(item.first);
  }
}
#endif

TimerSPtr TimerManager::CreateHeartbeatTimer(const RaftAddr &dest) {
  assert(maketimer_func_);
  TimerSPtr sptr;
  TimerParam param;
  param.timeout_ms = 0;
  param.repeat_ms = heartbeat_ms_;
  param.cb = heartbeat_func_;
  param.data = data_;
  param.name = "heartbeat-timer";
  sptr = maketimer_func_(param);
  sptr->set_dest_addr(dest.ToU64());
  return sptr;
}

void TimerManager::AddHeartbeatTimer(const RaftAddr &dest, TimerSPtr sptr) {
  auto it = heartbeats_.find(dest.ToU64());
  assert(it == heartbeats_.end());
  assert(dest.ToU64() == sptr->dest_addr());
  heartbeats_[dest.ToU64()] = sptr;
}

void TimerManager::MakeHeartbeat() {
  for (auto &item : heartbeats_) {
    RaftAddr addr(item.first);
    TimerSPtr sptr = CreateHeartbeatTimer(addr);
    item.second = sptr;
  }
}

#if 0
void TimerManager::MakeHeartbeat() {
  assert(maketimer_func_);
  for (auto &item : heartbeats_) {
    TimerParam param;
    param.timeout_ms = 0;
    param.repeat_ms = heartbeat_ms_;
    param.cb = heartbeat_func_;
    param.data = data_;
    param.name = "heartbeat-timer";
    item.second = maketimer_func_(param);
    item.second->set_dest_addr(item.first);
  }
}
#endif

void TimerManager::StartTick() { tick_->Start(); }

void TimerManager::StartElection() {
  next_election_ms_ = random_election_ms_.Get();
  election_->Again(0, next_election_ms_);
}

void TimerManager::AgainElection() {
  last_election_ms_ = next_election_ms_;
  next_election_ms_ = random_election_ms_.Get();
  election_->Again(next_election_ms_, 0);
}

void TimerManager::StartRequestVote() {
  for (auto &item : request_votes_) {
    if (item.second) {
      item.second->Start();
    }
  }
}

void TimerManager::StartRequestVote(uint64_t addr) {
  auto it = request_votes_.find(addr);
  if (it != request_votes_.end()) {
    if (it->second) {
      it->second->Start();
    }
  }
}

void TimerManager::StartHeartBeat() {
  for (auto &item : heartbeats_) {
    if (item.second) {
      item.second->Again(0, heartbeat_ms_);
    }
  }
}

void TimerManager::StartHeartBeat(uint64_t addr) {
  auto it = heartbeats_.find(addr);
  if (it != heartbeats_.end()) {
    if (it->second) {
      it->second->Again(0, heartbeat_ms_);
    }
  }
}

void TimerManager::AgainHeartBeat() {
  for (auto &item : heartbeats_) {
    if (item.second) {
      item.second->Again(heartbeat_ms_, heartbeat_ms_);
    }
  }
}

void TimerManager::AgainHeartBeat(uint64_t addr) {
  auto it = heartbeats_.find(addr);
  if (it != heartbeats_.end()) {
    if (it->second) {
      it->second->Again(heartbeat_ms_, heartbeat_ms_);
    }
  }
}

void TimerManager::Stop() {
  StopTick();
  StopElection();
  StopRequestVote();
  StopHeartBeat();
}

void TimerManager::StopTick() { tick_->Stop(); }

void TimerManager::StopElection() { election_->Stop(); }

void TimerManager::StopRequestVote() {
  for (auto &item : request_votes_) {
    if (item.second) {
      item.second->Stop();
    }
  }
}

void TimerManager::StopRequestVote(uint64_t addr) {
  auto it = request_votes_.find(addr);
  if (it != request_votes_.end()) {
    if (it->second) {
      it->second->Stop();
    }
  }
}

void TimerManager::StopHeartBeat() {
  for (auto &item : heartbeats_) {
    if (item.second) {
      item.second->Stop();
    }
  }
}

void TimerManager::StopHeartBeat(uint64_t addr) {
  auto it = heartbeats_.find(addr);
  if (it != heartbeats_.end()) {
    if (it->second) {
      it->second->Stop();
    }
  }
}

void TimerManager::Close() {
  CloseTick();
  CloseElection();
  CloseRequestVote();
  CloseHeartBeat();
}

void TimerManager::CloseTick() { tick_->Close(); }

void TimerManager::CloseElection() { election_->Close(); }

void TimerManager::CloseRequestVote() {
  for (auto &item : request_votes_) {
    if (item.second) {
      item.second->Close();
    }
  }
}

void TimerManager::CloseRequestVote(uint64_t addr) {
  auto it = request_votes_.find(addr);
  if (it != request_votes_.end()) {
    if (it->second) {
      it->second->Close();
    }
  }
}

void TimerManager::CloseHeartBeat() {
  for (auto &item : heartbeats_) {
    if (item.second) {
      item.second->Close();
    }
  }
}

void TimerManager::CloseHeartBeat(uint64_t addr) {
  auto it = heartbeats_.find(addr);
  if (it != heartbeats_.end()) {
    if (it->second) {
      it->second->Close();
    }
  }
}

}  // namespace vraft
