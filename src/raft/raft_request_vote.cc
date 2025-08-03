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
\* Message handlers
\* i = recipient, j = sender, m = message

\* Server i receives a RequestVote request from server j with
\* m.mterm <= currentTerm[i].
HandleRequestVoteRequest(i, j, m) ==
    LET logOk == \/ m.mlastLogTerm > LastTerm(log[i])
                 \/ /\ m.mlastLogTerm = LastTerm(log[i])
                    /\ m.mlastLogIndex >= Len(log[i])
        grant == /\ m.mterm = currentTerm[i]
                 /\ logOk
                 /\ votedFor[i] \in {Nil, j}
    IN /\ m.mterm <= currentTerm[i]
       /\ \/ grant  /\ votedFor' = [votedFor EXCEPT ![i] = j]
          \/ ~grant /\ UNCHANGED votedFor
       /\ Reply([mtype        |-> RequestVoteResponse,
                 mterm        |-> currentTerm[i],
                 mvoteGranted |-> grant,
                 \* mlog is used just for the `elections' history variable for
                 \* the proof. It would not exist in a real implementation.
                 mlog         |-> log[i],
                 msource      |-> i,
                 mdest        |-> j],
                 m)
       /\ UNCHANGED <<state, currentTerm, candidateVars, leaderVars, logVars>>

********************************************************************************************/
int32_t Raft::OnRequestVote(struct RequestVote &msg) {
  if (started_) {
    Tracer tracer(this, true, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    RequestVoteReply reply;
    reply.src = msg.dest;
    reply.dest = msg.src;
    reply.uid = UniqId(&reply);
    reply.pre_vote = msg.pre_vote;
    reply.interval_ok = true;
    reply.req_term = msg.term;
    reply.elapse = 0;

    RaftIndex last_index = LastIndex();
    RaftTerm last_term = LastTerm();
    bool log_ok =
        ((msg.last_log_term > last_term) ||
         (msg.last_log_term == last_term && msg.last_log_index >= last_index));

    if (interval_check_) {
      if (!msg.leader_transfer) {
        bool interval_ok = (Clock::NSec() - last_heartbeat_timestamp_ >=
                            timer_mgr_.election_ms() * 1000 * 1000);

        // if !interval_ok, response reject
        if (!interval_ok) {
          reply.term = meta_.term();
          reply.send_ts = Clock::NSec();
          reply.granted = false;
          reply.log_ok = log_ok;
          reply.interval_ok = false;

          goto end;
        }
      }
    }

    // maybe step down first
    if (msg.term > meta_.term()) {
      StepDown(msg.term, &tracer);
    }
    assert(msg.term <= meta_.term());

    // if grant
    if (msg.term == meta_.term()) {
      if (log_ok && meta_.vote() == 0) {  // give my vote
        StepDown(meta_.term(), &tracer);

        // reset election
        timer_mgr_.StopRequestVote();
        timer_mgr_.AgainElection();

        // vote
        if (!msg.pre_vote) {
          meta_.SetVote(msg.src.ToU64());
        }
      }

    } else {
      // msg.term < meta_.term()
      // reject
    }

    // reply
    {
      reply.term = meta_.term();
      reply.send_ts = Clock::NSec();
      reply.granted =
          (msg.term == meta_.term() && meta_.vote() == msg.src.ToU64());
      reply.log_ok = log_ok;
    }

  end:
    SendRequestVoteReply(reply, &tracer);
    tracer.PrepareState1();
    tracer.Finish();
  }

  return 0;
}

/********************************************************************************************
\* Server i receives a RequestVote response from server j with
\* m.mterm = currentTerm[i].
HandleRequestVoteResponse(i, j, m) ==
    \* This tallies votes even when the current state is not Candidate, but
    \* they won't be looked at, so it doesn't matter.
    /\ m.mterm = currentTerm[i]
    /\ votesResponded' = [votesResponded EXCEPT ![i] =
                              votesResponded[i] \cup {j}]
    /\ \/ /\ m.mvoteGranted
          /\ votesGranted' = [votesGranted EXCEPT ![i] =
                                  votesGranted[i] \cup {j}]
          /\ voterLog' = [voterLog EXCEPT ![i] =
                              voterLog[i] @@ (j :> m.mlog)]
       \/ /\ ~m.mvoteGranted
          /\ UNCHANGED <<votesGranted, voterLog>>
    /\ Discard(m)
    /\ UNCHANGED <<serverVars, votedFor, leaderVars, logVars>>
********************************************************************************************/
int32_t Raft::OnRequestVoteReply(struct RequestVoteReply &msg) {
  if (started_) {
    Tracer tracer(this, true, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    // drop stale
    if (msg.term < meta_.term()) {
      char buf[128];
      snprintf(buf, sizeof(buf), "drop stale response, term %lu < %lu",
               msg.term, meta_.term());
      tracer.PrepareEvent(kEventOther, std::string(buf));
      goto end;
    }

    // drop response, rpc term not equal
    if (msg.req_term != meta_.term()) {
      char buf[128];
      snprintf(buf, sizeof(buf),
               "drop response, rpc term not equal, current-term %lu != "
               "msg.req-term:%lu",
               meta_.term(), msg.req_term);
      tracer.PrepareEvent(kEventOther, std::string(buf));
      goto end;
    }

    if (msg.term > meta_.term()) {  // step down
      StepDown(msg.term, &tracer);

    } else {  // process
      assert(msg.term == meta_.term());

      if (!msg.pre_vote) {  // real vote
        // get response
        vote_mgr_.Done(msg.src.ToU64());

        // close rpc timer
        timer_mgr_.StopRequestVote(msg.src.ToU64());

        if (msg.granted) {
          // get vote
          vote_mgr_.GetVote(msg.src.ToU64());

          if (vote_mgr_.Majority(IfSelfVote()) && state_ == STATE_CANDIDATE) {
            BecomeLeader(&tracer);
          }
        }

      } else {  // pre-vote
        // close rpc timer
        timer_mgr_.StopRequestVote(msg.src.ToU64());

        // set log ok
        if (msg.log_ok) {
          vote_mgr_.LogOK(msg.src.ToU64());
        }

        // set interval ok
        if (msg.interval_ok) {
          vote_mgr_.IntervalOK(msg.src.ToU64());
        }

        // check pre-vote ok
        if (msg.log_ok) {
          if (vote_mgr_.MajorityPreVoteOK(true) && state_ == STATE_CANDIDATE &&
              pre_voting_) {
            // clear pre-voting flag
            pre_voting_ = false;

            // do real request vote
            DoRequestVote(&tracer);
          }
        }
      }
    }

  end:
    tracer.PrepareState1();
    tracer.Finish();
  }
  return 0;
}

int32_t Raft::SendRequestVote(uint64_t dest, Tracer *tracer) {
  RequestVote msg;
  msg.src = Me();
  msg.dest = RaftAddr(dest);
  msg.term = meta_.term();
  msg.uid = UniqId(&msg);
  msg.send_ts = Clock::NSec();
  msg.elapse = 0;

  msg.last_log_index = LastIndex();
  msg.last_log_term = LastTerm();
  msg.pre_vote = pre_voting_;

  if (leader_transfer_ && meta_.term() <= transfer_max_term_) {
    msg.leader_transfer = true;
  } else {
    msg.leader_transfer = false;
    leader_transfer_ = false;
    transfer_max_term_ = 0;
  }

  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kRequestVote;
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

int32_t Raft::SendRequestVoteReply(RequestVoteReply &msg, Tracer *tracer) {
  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kRequestVoteReply;
  std::string header_str;
  header.ToString(header_str);

  if (send_) {
    header_str.append(std::move(body_str));
    int32_t rv = send_(msg.dest.ToU64(), header_str.data(), header_str.size());

    if (tracer != nullptr && rv == 0) {
      tracer->PrepareEvent(kEventSend, msg.ToJsonString(false, true));
    }
  }

  return 0;
}

}  // namespace vraft
