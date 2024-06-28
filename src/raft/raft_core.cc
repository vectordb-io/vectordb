#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "clock.h"
#include "raft.h"
#include "raft_server.h"
#include "util.h"
#include "vraft_logger.h"

namespace vraft {

/********************************************************************************************
\* Server i times out and starts a new election.
Timeout(i) == /\ state[i] \in {Follower, Candidate}
              /\ state' = [state EXCEPT ![i] = Candidate]
              /\ currentTerm' = [currentTerm EXCEPT ![i] = currentTerm[i] + 1]
              \* Most implementations would probably just set the local vote
              \* atomically, but messaging localhost for it is weaker.
              /\ votedFor' = [votedFor EXCEPT ![i] = Nil]
              /\ votesResponded' = [votesResponded EXCEPT ![i] = {}]
              /\ votesGranted'   = [votesGranted EXCEPT ![i] = {}]
              /\ voterLog'       = [voterLog EXCEPT ![i] = [j \in {} |-> <<>>]]
              /\ UNCHANGED <<messages, leaderVars, logVars>>
********************************************************************************************/
void Elect(Timer *timer) {
  Raft *r = reinterpret_cast<Raft *>(timer->data());
  assert(r->state_ == FOLLOWER || r->state_ == CANDIDATE);

  Tracer tracer(r, true, r->tracer_cb_);
  tracer.PrepareState0();

  r->meta_.IncrTerm();
  r->state_ = CANDIDATE;
  r->leader_ = RaftAddr(0);

  // reset candidate state, vote-manager
  r->vote_mgr_.Clear();

  // vote for myself
  r->meta_.SetVote(r->Me().ToU64());

  // only myself, become leader
  if (r->vote_mgr_.QuorumAll(r->IfSelfVote())) {
    r->BecomeLeader(&tracer);
    return;
  }

  tracer.PrepareEvent(kEventTimer, "election-timer timeout");
  tracer.PrepareState1();
  tracer.Finish();

  // start request-vote
  r->timer_mgr_.StartRequestVote();

  // reset election timer
  r->timer_mgr_.AgainElection();
}

/********************************************************************************************
\* Candidate i sends j a RequestVote request.
RequestVote(i, j) ==
    /\ state[i] = Candidate
    /\ j \notin votesResponded[i]
    /\ Send([mtype         |-> RequestVoteRequest,
             mterm         |-> currentTerm[i],
             mlastLogTerm  |-> LastTerm(log[i]),
             mlastLogIndex |-> Len(log[i]),
             msource       |-> i,
             mdest         |-> j])
    /\ UNCHANGED <<serverVars, candidateVars, leaderVars, logVars>>
********************************************************************************************/
void RequestVoteRpc(Timer *timer) {
  Raft *r = reinterpret_cast<Raft *>(timer->data());
  assert(r->state_ == CANDIDATE);

  Tracer tracer(r, true, r->tracer_cb_);
  tracer.PrepareState0();

  int32_t rv = r->SendRequestVote(timer->dest_addr(), &tracer);
  assert(rv == 0);

  tracer.PrepareState1();
  tracer.Finish();
}

int32_t Raft::SendRequestVote(uint64_t dest, Tracer *tracer) {
  RequestVote msg;
  msg.src = Me();
  msg.dest = RaftAddr(dest);
  msg.term = meta_.term();
  msg.uid = UniqId(&msg);

  msg.last_log_index = LastIndex();
  msg.last_log_term = LastTerm();

  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kRequestVote;
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

/********************************************************************************************
\* Leader i sends j an AppendEntries request containing up to 1 entry.
\* While implementations may want to send more than 1 at a time, this spec uses
\* just 1 because it minimizes atomic regions without loss of generality.
AppendEntries(i, j) ==
    /\ i /= j
    /\ state[i] = Leader
    /\ LET prevLogIndex == nextIndex[i][j] - 1
           prevLogTerm == IF prevLogIndex > 0 THEN
                              log[i][prevLogIndex].term
                          ELSE
                              0
           \* Send up to 1 entry, constrained by the end of the log.
           lastEntry == Min({Len(log[i]), nextIndex[i][j]})
           entries == SubSeq(log[i], nextIndex[i][j], lastEntry)
       IN Send([mtype          |-> AppendEntriesRequest,
                mterm          |-> currentTerm[i],
                mprevLogIndex  |-> prevLogIndex,
                mprevLogTerm   |-> prevLogTerm,
                mentries       |-> entries,
                \* mlog is used as a history variable for the proof.
                \* It would not exist in a real implementation.
                mlog           |-> log[i],
                mcommitIndex   |-> Min({commitIndex[i], lastEntry}),
                msource        |-> i,
                mdest          |-> j])
    /\ UNCHANGED <<serverVars, candidateVars, leaderVars, logVars>>
********************************************************************************************/
void HeartBeat(Timer *timer) {
  Raft *r = reinterpret_cast<Raft *>(timer->data());
  assert(r->state_ == LEADER);

  Tracer tracer(r, true, r->tracer_cb_);
  tracer.PrepareState0();

  tracer.PrepareEvent(kEventTimer, "heartbeat-timer timeout");
  int32_t rv = r->SendAppendEntries(timer->dest_addr(), &tracer);
  assert(rv == 0);

  tracer.PrepareState1();
  tracer.Finish();
}

int32_t Raft::SendAppendEntries(uint64_t dest, Tracer *tracer) {
  RaftIndex last_index = LastIndex();
  RaftIndex next_index = index_mgr_.GetNext(dest);
  RaftIndex pre_index = next_index - 1;
  assert(pre_index <= last_index);

  if (next_index < log_.First()) {
    // do not have log, send snapshot
    assert(0);
    return 0;
  }
  assert(next_index >= log_.First());

  RaftTerm pre_term = 0;
  if (pre_index >= log_.First()) {
    pre_term = GetTerm(pre_index);

  } else if (pre_index == 0) {
    pre_term = 0;

  } else if (sm_ && pre_index == sm_->LastIndex()) {
    pre_term = sm_->LastTerm();

  } else {
    // do not have log, send snapshot
    assert(0);
    return 0;
  }

  AppendEntries msg;
  msg.src = Me();
  msg.dest = RaftAddr(dest);
  msg.term = meta_.term();
  msg.uid = UniqId(&msg);
  msg.pre_log_index = pre_index;
  msg.pre_log_term = pre_term;

  if (log_.IndexValid(next_index)) {
    LogEntry entry;
    int32_t rv = log_.Get(next_index, entry);
    assert(rv == 0);
    msg.entries.push_back(entry);
  }

  // tla+
  // mcommitIndex   |-> Min({commitIndex[i], lastEntry}),
  // logcabin:
  // request.set_commit_index(std::min(commitIndex, prevLogIndex + numEntries));
  msg.commit_index =
      std::min(commit_, pre_index + static_cast<RaftIndex>(msg.entries.size()));

  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kAppendEntries;
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

int32_t Raft::SendInstallSnapshot(uint64_t dest, Tracer *tracer) { return 0; }

/********************************************************************************************
\* Leader i receives a client request to add v to the log.
ClientRequest(i, v) ==
    /\ state[i] = Leader
    /\ LET entry == [term  |-> currentTerm[i],
                     value |-> v]
           newLog == Append(log[i], entry)
       IN  log' = [log EXCEPT ![i] = newLog]
    /\ UNCHANGED <<messages, serverVars, candidateVars,
                   leaderVars, commitIndex>>
********************************************************************************************/
int32_t Raft::Propose(std::string value, Functor cb) {
  if (assert_loop_) {
    assert_loop_();
  }

  Tracer tracer(this, true, tracer_cb_);
  tracer.PrepareState0();
  char buf[128];
  snprintf(buf, sizeof(buf), "propose value, length:%lu", value.size());
  tracer.PrepareEvent(kEventOther, std::string(buf));

  int32_t rv = 0;
  AppendEntry entry;

  if (state_ != LEADER) {
    rv = -1;
    goto end;
  }

  entry.term = meta_.term();
  entry.type = kData;
  entry.value = value;
  rv = log_.AppendOne(entry, &tracer);
  assert(rv == 0);

  MaybeCommit(&tracer);
  if (config_mgr_.Current().peers.size() > 0) {
    for (auto &peer : config_mgr_.Current().peers) {
      rv = SendAppendEntries(peer.ToU64(), &tracer);
      assert(rv == 0);

      timer_mgr_.AgainHeartBeat(peer.ToU64());
    }
  }

end:
  tracer.PrepareState1();
  tracer.Finish();
  return rv;
}

/********************************************************************************************
\* The term of the last entry in a log, or 0 if the log is empty.
LastTerm(xlog) == IF Len(xlog) = 0 THEN 0 ELSE xlog[Len(xlog)].term
********************************************************************************************/
RaftTerm Raft::LastTerm() {
  RaftIndex snapshot_last = 0;
  if (sm_) {
    snapshot_last = sm_->LastIndex();
  }
  RaftIndex log_last = log_.Last();

  if (log_last >= snapshot_last) {  // log is newer
    if (log_last == 0) {            // no log, no snapshot
      return 0;

    } else {                         // has log
      return log_.LastMeta()->term;  // return last log term
    }

  } else {  // snapshot is newer
    assert(sm_);
    return sm_->LastTerm();
  }
}

/********************************************************************************************
\* Any RPC with a newer term causes the recipient to advance its term first.
UpdateTerm(i, j, m) ==
    /\ m.mterm > currentTerm[i]
    /\ currentTerm'    = [currentTerm EXCEPT ![i] = m.mterm]
    /\ state'          = [state       EXCEPT ![i] = Follower]
    /\ votedFor'       = [votedFor    EXCEPT ![i] = Nil]
       \* messages is unchanged so m can be processed further.
    /\ UNCHANGED <<messages, candidateVars, leaderVars, logVars>>
********************************************************************************************/
void Raft::StepDown(RaftTerm new_term, Tracer *tracer) {
  if (tracer != nullptr) {
    char buf[128];
    snprintf(buf, sizeof(buf), "step down, term %ld >= %ld", new_term,
             meta_.term());
    tracer->PrepareEvent(kEventOther, std::string(buf));
  }

  assert(meta_.term() <= new_term);
  if (meta_.term() < new_term) {  // larger term
    meta_.SetTerm(new_term);
    meta_.SetVote(0);
    leader_ = RaftAddr(0);
    state_ = FOLLOWER;

  } else {  // equal term
    if (state_ != FOLLOWER) {
      state_ = FOLLOWER;
    }
  }

  // close heartbeat timer
  timer_mgr_.StopHeartBeat();

  // start election timer
  timer_mgr_.StopRequestVote();
  timer_mgr_.AgainElection();
}

/********************************************************************************************
\* Candidate i transitions to leader.
BecomeLeader(i) ==
    /\ state[i] = Candidate
    /\ votesGranted[i] \in Quorum
    /\ state'      = [state EXCEPT ![i] = Leader]
    /\ nextIndex'  = [nextIndex EXCEPT ![i] =
                         [j \in Server |-> Len(log[i]) + 1]]
    /\ matchIndex' = [matchIndex EXCEPT ![i] =
                         [j \in Server |-> 0]]
    /\ elections'  = elections \cup
                         {[eterm     |-> currentTerm[i],
                           eleader   |-> i,
                           elog      |-> log[i],
                           evotes    |-> votesGranted[i],
                           evoterLog |-> voterLog[i]]}
    /\ UNCHANGED <<messages, currentTerm, votedFor, candidateVars, logVars>>
********************************************************************************************/
void Raft::BecomeLeader(Tracer *tracer) {
  if (tracer != nullptr) {
    tracer->PrepareEvent(kEventOther, "become leader");
  }

  assert(state_ == CANDIDATE);
  state_ = LEADER;
  leader_ = Me().ToU64();

  // stop election timer
  timer_mgr_.StopElection();
  timer_mgr_.StopRequestVote();

  // reset leader state, index-manager
  index_mgr_.ResetNext(LastIndex() + 1);
  index_mgr_.ResetMatch(0);

  // start heartbeat timer
  timer_mgr_.StartHeartBeat();

  // append noop
  AppendNoop(tracer);
}

/********************************************************************************************
\* Leader i advances its commitIndex.
\* This is done as a separate step from handling AppendEntries responses,
\* in part to minimize atomic regions, and in part so that leaders of
\* single-server clusters are able to mark entries committed.
AdvanceCommitIndex(i) ==
    /\ state[i] = Leader
    /\ LET \* The set of servers that agree up through index.
           Agree(index) == {i} \cup {k \in Server :
                                         matchIndex[i][k] >= index}
           \* The maximum indexes for which a quorum agrees
           agreeIndexes == {index \in 1..Len(log[i]) :
                                Agree(index) \in Quorum}
           \* New value for commitIndex'[i]
           newCommitIndex ==
              IF /\ agreeIndexes /= {}
                 /\ log[i][Max(agreeIndexes)].term = currentTerm[i]
              THEN
                  Max(agreeIndexes)
              ELSE
                  commitIndex[i]
       IN commitIndex' = [commitIndex EXCEPT ![i] = newCommitIndex]
    /\ UNCHANGED <<messages, serverVars, candidateVars, leaderVars, log>>
********************************************************************************************/
void Raft::MaybeCommit(Tracer *tracer) {
  assert(state_ == LEADER);
  uint64_t new_commit = index_mgr_.MajorityMax(LastIndex());
  if (commit_ >= new_commit) {
    return;
  }

  assert(new_commit > commit_);
  assert(new_commit >= log_.First());

  MetaValue new_commit_meta;
  int32_t rv = log_.GetMeta(new_commit, new_commit_meta);
  assert(rv == 0);

  if (new_commit_meta.term != meta_.term()) {
    return;
  }

  RaftIndex old_commit = commit_;  // record for tracer
  commit_ = new_commit;
  assert(commit_ <= log_.Last());

  // state machine apply
  StateMachineApply(tracer);

  if (tracer != nullptr) {
    char buf[128];
    snprintf(buf, sizeof(buf), "advance commit from %u to %u", old_commit,
             commit_);
    tracer->PrepareEvent(kEventOther, std::string(buf));
  }
}

void Raft::StateMachineApply(Tracer *tracer) {
  if (tracer) {
    char buf[128];
    snprintf(buf, sizeof(buf), "state-machine apply, last-apply:%u, commit:%u",
             last_apply_, commit_);
    tracer->PrepareEvent(kEventOther, std::string(buf));
  }

  // state machine apply
  if (commit_ > last_apply_) {
    if (sm_) {
      for (RaftIndex i = last_apply_ + 1; i <= commit_; ++i) {
        LogEntry log_entry;
        int32_t rv = log_.Get(i, log_entry);
        assert(rv == 0);

        if (log_entry.append_entry.type == kData) {
          rv = sm_->Apply(&log_entry, Me());
          assert(rv == 0);

          // propose call back with rv
        }
      }
    }
    last_apply_ = commit_;
  }
}

}  // namespace vraft
