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
  assert(r->state_ == STATE_FOLLOWER || r->state_ == STATE_CANDIDATE);

  if (r->standby()) {
    // reset election timer
    r->AgainElection();
    return;
  }

  Tracer tracer(r, true, r->tracer_cb_);
  tracer.PrepareState0();

  std::string str = r->Me().ToString() + std::string(" election-timer timeout");
  tracer.PrepareEvent(kEventTimer, str);

  if (r->enable_pre_vote() && r->Peers().size() > 0) {
    r->set_pre_voting(true);
    r->DoPreVote(&tracer);

  } else {
    r->set_pre_voting(false);
    r->DoRequestVote(&tracer);
  }

  tracer.PrepareState1();
  tracer.Finish();
}

void Raft::DoRequestVote(Tracer *tracer) {
  // increase term
  RaftTerm old_term = meta_.term();
  meta_.IncrTerm();
  RaftTerm new_term = meta_.term();

  if (tracer) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s increase-term term:%lu_to_%lu",
             Me().ToString().c_str(), old_term, new_term);
    tracer->PrepareEvent(kEventOther, std::string(buf));
  }

  // become candidate
  BecomeCandidate(tracer);

  // vote for myself
  meta_.SetVote(Me().ToU64());

  // only myself, become leader
  if (vote_mgr_.QuorumAll(IfSelfVote())) {
    BecomeLeader(tracer);
    return;
  }

  // start request-vote
  timer_mgr_.StartRequestVote();

  // reset election timer
  timer_mgr_.AgainElection();
}

void Raft::DoPreVote(Tracer *tracer) {
  // become candidate
  BecomeCandidate(tracer);

  // start request-vote
  timer_mgr_.StartRequestVote();

  // reset election timer
  timer_mgr_.AgainElection();
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
  assert(r->state_ == STATE_CANDIDATE);

  Tracer tracer(r, true, r->tracer_cb_);
  tracer.PrepareState0();

  int32_t rv = r->SendRequestVote(timer->dest_addr(), &tracer);
  assert(rv == 0);

  tracer.PrepareState1();
  tracer.Finish();
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
  assert(r->state_ == STATE_LEADER);

  Tracer tracer(r, true, r->tracer_cb_);
  tracer.PrepareState0();

  std::string str =
      r->Me().ToString() + std::string(" heartbeat-timer timeout");
  tracer.PrepareEvent(kEventTimer, str);
  int32_t rv = r->SendAppendEntries(timer->dest_addr(), &tracer);
  assert(rv == 0);

  tracer.PrepareState1();
  tracer.Finish();
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
  State old_state = state_;

  assert(meta_.term() <= new_term);
  if (meta_.term() < new_term) {  // larger term
    meta_.SetTerm(new_term);
    meta_.SetVote(0);
    leader_ = RaftAddr(0);
    state_ = STATE_FOLLOWER;

  } else {  // equal term
    if (state_ != STATE_FOLLOWER) {
      state_ = STATE_FOLLOWER;
    }
  }

  // close heartbeat timer
  timer_mgr_.StopHeartBeat();

  // start election timer
  timer_mgr_.StopRequestVote();
  timer_mgr_.AgainElection();

  if (last_heartbeat_timestamp_ == INT64_MAX) {
    last_heartbeat_timestamp_ = 0;
  }

  if (tracer != nullptr) {
    RaftTerm old_term = meta_.term();
    char buf[128];
    snprintf(buf, sizeof(buf), "%s step-down term:%lu_to_%lu %s_to_%s",
             Me().ToString().c_str(), old_term, new_term, StateToStr(old_state),
             StateToStr(state_));
    tracer->PrepareEvent(kEventOther, std::string(buf));
  }
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
  State old_state = state_;

  leader_transfer_ = false;

  assert(state_ == STATE_CANDIDATE);
  state_ = STATE_LEADER;
  leader_ = Me();
  ++leader_times_;

  // update last_heartbeat_timestamp
  last_heartbeat_timestamp_ = INT64_MAX;

  // stop election timer
  timer_mgr_.StopElection();
  timer_mgr_.StopRequestVote();

  // reset leader state, index-manager
  index_mgr_.ResetNext(LastIndex() + 1);
  index_mgr_.ResetMatch(0);

  // start heartbeat timer
  timer_mgr_.StartHeartBeat();

  if (tracer != nullptr) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s become-leader term:%lu %s_to_%s",
             Me().ToString().c_str(), meta_.term(), StateToStr(old_state),
             StateToStr(state_));
    tracer->PrepareEvent(kEventOther, std::string(buf));
  }

  // append noop
  AppendNoop(tracer);
}

void Raft::BecomeCandidate(Tracer *tracer) {
  State old_state = state_;

  state_ = STATE_CANDIDATE;
  leader_ = RaftAddr(0);

  // reset candidate state, vote-manager
  vote_mgr_.Clear();

  if (tracer != nullptr) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s become-candidate term:%lu %s_to_%s",
             Me().ToString().c_str(), meta_.term(), StateToStr(old_state),
             StateToStr(state_));
    tracer->PrepareEvent(kEventOther, std::string(buf));
  }
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
  assert(state_ == STATE_LEADER);
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
    for (RaftIndex i = last_apply_ + 1; i <= commit_; ++i) {
      LogEntry log_entry;
      int32_t rv = log_.Get(i, log_entry);
      assert(rv == 0);

      if (log_entry.append_entry.type == kData) {
        if (sm_) {
          rv = sm_->Apply(&log_entry, Me());
          assert(rv == 0);
        }
        // propose call back with rv

      } else if (log_entry.append_entry.type == kConfig) {
        if (changing_index_ == log_entry.index) {
          changing_index_ = 0;

          if (tracer) {
            RaftConfig rc;
            rc.FromString(log_entry.append_entry.value);

            char buf[256];
            snprintf(buf, sizeof(buf), "%s config-change-finish index:%u %s",
                     Me().ToString().c_str(), i,
                     rc.ToJsonString(true, true).c_str());
            tracer->PrepareEvent(kEventOther, std::string(buf));
          }

          standby_ = false;
        }

        // cb

      } else if (log_entry.append_entry.type == kNoop) {
      } else {
        assert(0);
      }
    }

    last_apply_ = commit_;
  }
}

}  // namespace vraft
