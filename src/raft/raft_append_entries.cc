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
\* Server i receives an AppendEntries request from server j with
\* m.mterm <= currentTerm[i]. This just handles m.entries of length 0 or 1, but
\* implementations could safely accept more by treating them the same as
\* multiple independent requests of 1 entry.
HandleAppendEntriesRequest(i, j, m) ==
    LET logOk == \/ m.mprevLogIndex = 0
                 \/ /\ m.mprevLogIndex > 0
                    /\ m.mprevLogIndex <= Len(log[i])
                    /\ m.mprevLogTerm = log[i][m.mprevLogIndex].term
    IN /\ m.mterm <= currentTerm[i]
       /\ \/ /\ \* reject request
                \/ m.mterm < currentTerm[i]
                \/ /\ m.mterm = currentTerm[i]
                   /\ state[i] = Follower
                   /\ \lnot logOk
             /\ Reply([mtype           |-> AppendEntriesResponse,
                       mterm           |-> currentTerm[i],
                       msuccess        |-> FALSE,
                       mmatchIndex     |-> 0,
                       msource         |-> i,
                       mdest           |-> j],
                       m)
             /\ UNCHANGED <<serverVars, logVars>>
          \/ \* return to follower state
             /\ m.mterm = currentTerm[i]
             /\ state[i] = Candidate
             /\ state' = [state EXCEPT ![i] = Follower]
             /\ UNCHANGED <<currentTerm, votedFor, logVars, messages>>
          \/ \* accept request
             /\ m.mterm = currentTerm[i]
             /\ state[i] = Follower
             /\ logOk
             /\ LET index == m.mprevLogIndex + 1
                IN \/ \* already done with request
                       /\ \/ m.mentries = << >>
                          \/ /\ m.mentries /= << >>
                             /\ Len(log[i]) >= index
                             /\ log[i][index].term = m.mentries[1].term
                          \* This could make our commitIndex decrease (for
                          \* example if we process an old, duplicated request),
                          \* but that doesn't really affect anything.
                       /\ commitIndex' = [commitIndex EXCEPT ![i] =
                                              m.mcommitIndex]
                       /\ Reply([mtype           |-> AppendEntriesResponse,
                                 mterm           |-> currentTerm[i],
                                 msuccess        |-> TRUE,
                                 mmatchIndex     |-> m.mprevLogIndex +
                                                     Len(m.mentries),
                                 msource         |-> i,
                                 mdest           |-> j],
                                 m)
                       /\ UNCHANGED <<serverVars, log>>
                   \/ \* conflict: remove 1 entry
                       /\ m.mentries /= << >>
                       /\ Len(log[i]) >= index
                       /\ log[i][index].term /= m.mentries[1].term
                       /\ LET new == [index2 \in 1..(Len(log[i]) - 1) |->
                                          log[i][index2]]
                          IN log' = [log EXCEPT ![i] = new]
                       /\ UNCHANGED <<serverVars, commitIndex, messages>>
                   \/ \* no conflict: append entry
                       /\ m.mentries /= << >>
                       /\ Len(log[i]) = m.mprevLogIndex
                       /\ log' = [log EXCEPT ![i] =
                                      Append(log[i], m.mentries[1])]
                       /\ UNCHANGED <<serverVars, commitIndex, messages>>
       /\ UNCHANGED <<candidateVars, leaderVars>>
********************************************************************************************/
int32_t Raft::OnAppendEntries(struct AppendEntries &msg) {
  if (started_) {
    Tracer tracer(this, true, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    // temp variable used behind, define here due to "goto", make compiler happy
    RaftIndex index;

    // set response to a rejection
    // if we accept, we will overwrite it
    AppendEntriesReply reply;
    reply.src = msg.dest;
    reply.dest = msg.src;
    reply.term = meta_.term();
    reply.uid = UniqId(&reply);
    reply.success = false;
    reply.last_log_index = LastIndex();
    reply.req_pre_index = msg.pre_log_index;
    reply.req_num_entries = msg.entries.size();
    reply.req_term = msg.term;

    // stale term, send reply
    if (msg.term < meta_.term()) {
      char buf[128];
      snprintf(buf, sizeof(buf), "reject, stale term %lu < %lu", msg.term,
               meta_.term());
      tracer.PrepareEvent(kEventOther, std::string(buf));

      goto end;
    }

    // overwrite reply term
    if (msg.term > meta_.term()) {
      reply.term = msg.term;
    }

    // step down
    StepDown(msg.term, &tracer);

    // if get here, means we find a new leader
    // reset election timer immediately
    timer_mgr_.StopRequestVote();
    timer_mgr_.AgainElection();

    // update leader cache
    if (leader_.ToU64() == 0) {
      leader_ = msg.src;
    } else {
      assert(leader_.ToU64() == msg.src.ToU64());
    }

    // leave a gap, reject
    if (msg.pre_log_index > LastIndex()) {
      char buf[128];
      snprintf(buf, sizeof(buf), "reject, leave a gap, msg-pre:%u, my-last:%u",
               msg.pre_log_index, LastIndex());
      tracer.PrepareEvent(kEventOther, std::string(buf));

      goto end;
    }
    assert(msg.pre_log_index <= LastIndex());

    // pre-term not match, reject
    if (log_.IndexValid(msg.pre_log_index)) {
      RaftTerm my_pre_term = GetTerm(msg.pre_log_index);
      if (my_pre_term != msg.pre_log_term) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "reject, term not match, msg-pre-term:%lu, my-pre-term:%lu",
                 msg.pre_log_term, my_pre_term);
        tracer.PrepareEvent(kEventOther, std::string(buf));

        goto end;
      }
    }

    // if we get here, means accept
    reply.success = true;

    // make log same, append new entries
    // do nothing with already matched entries, just retain them
    index = msg.pre_log_index;
    for (auto it = msg.entries.begin(); it != msg.entries.end(); ++it) {
      ++index;
      LogEntry &entry = *it;
      assert(entry.index == index);

      // maybe snapshot, continue
      if (index < log_.First()) {
        continue;
      }

      // maybe truncate log
      if (log_.Last() >= index) {
        MetaValue meta;
        int32_t rv = log_.GetMeta(index, meta);
        assert(rv == 0);

        // already match, continue
        if (meta.term == entry.append_entry.term) {
          continue;
        }

        assert(commit_ < index);

        rv = log_.DeleteFrom(index);
        assert(rv == 0);

        char buf[128];
        snprintf(buf, sizeof(buf), "truncate log from %u", index);
        tracer.PrepareEvent(kEventOther, std::string(buf));
      }

      // append this and all following entries.
      do {
        LogEntry &entry_to_append = *it;
        int32_t rv = log_.AppendOne(entry_to_append.append_entry, &tracer);
        assert(rv == 0);

        ++it;
        ++index;

      } while (it != msg.entries.end());

      // process over
      break;
    }

    // overwrite reply
    reply.last_log_index = LastIndex();

    // update commit index
    if (commit_ < msg.commit_index) {
      char buf[128];
      snprintf(buf, sizeof(buf), "update commit from %u to %u", commit_,
               msg.commit_index);
      tracer.PrepareEvent(kEventOther, std::string(buf));

      commit_ = msg.commit_index;
      assert(commit_ <= LastIndex());

      // state machine apply
      StateMachineApply(&tracer);
    }

    // reset election timer again
    timer_mgr_.StopRequestVote();
    timer_mgr_.AgainElection();

  end:
    SendAppendEntriesReply(reply, &tracer);
    tracer.PrepareState1();
    tracer.Finish();
  }
  return 0;
}

int32_t Raft::SendAppendEntriesReply(AppendEntriesReply &msg, Tracer *tracer) {
  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kAppendEntriesReply;
  std::string header_str;
  header.ToString(header_str);

  if (send_) {
    header_str.append(std::move(body_str));
    send_(msg.dest.ToU64(), header_str.data(), header_str.size());

    if (tracer != nullptr) {
      tracer->PrepareEvent(kEventSend, msg.ToJsonString(false, true));
    }
  }

  return 0;
}

/********************************************************************************************
\* Server i receives an AppendEntries response from server j with
\* m.mterm = currentTerm[i].
HandleAppendEntriesResponse(i, j, m) ==
    /\ m.mterm = currentTerm[i]
    /\ \/ /\ m.msuccess \* successful
          /\ nextIndex'  = [nextIndex  EXCEPT ![i][j] = m.mmatchIndex + 1]
          /\ matchIndex' = [matchIndex EXCEPT ![i][j] = m.mmatchIndex]
       \/ /\ \lnot m.msuccess \* not successful
          /\ nextIndex' = [nextIndex EXCEPT ![i][j] =
                               Max({nextIndex[i][j] - 1, 1})]
          /\ UNCHANGED <<matchIndex>>
    /\ Discard(m)
    /\ UNCHANGED <<serverVars, candidateVars, logVars, elections>>
********************************************************************************************/
int32_t Raft::OnAppendEntriesReply(struct AppendEntriesReply &msg) {
  if (started_) {
    Tracer tracer(this, true, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    // drop stale
    if (msg.term < meta_.term()) {
      char buf[128];
      snprintf(buf, sizeof(buf), "drop response, stale term %lu < %lu",
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

    assert((state_ == LEADER));

    if (msg.term > meta_.term()) {
      StepDown(msg.term, &tracer);

    } else {
      assert(msg.term == meta_.term());
      assert(msg.req_term == meta_.term());

      if (msg.success) {  // follower return match
        if (index_mgr_.GetMatch(msg.src) >
            msg.req_pre_index + msg.req_num_entries) {
          // maybe pipeline ?

        } else {
          assert(index_mgr_.GetMatch(msg.src) <=
                 msg.req_pre_index + msg.req_num_entries);

          // increase match index
          index_mgr_.SetMatch(msg.src, msg.req_pre_index + msg.req_num_entries);
          MaybeCommit(&tracer);
        }

        // increase next index
        index_mgr_.SetNext(msg.src, index_mgr_.GetMatch(msg.src) + 1);

      } else {  // follower return not match
        // decrease next index
        if (index_mgr_.GetNext(msg.src) > 1) {
          index_mgr_.DecrNext(msg.src);
        }

        // speed up next index!!
        if (index_mgr_.GetNext(msg.src) > msg.last_log_index + 1) {
          index_mgr_.SetNext(msg.src, msg.last_log_index + 1);
        }
      }

      // send reply immediately
      if (index_mgr_.GetNext(msg.src) <= LastIndex()) {
        SendAppendEntries(msg.src.ToU64(), &tracer);

        // reset heartbeat timer
        timer_mgr_.AgainHeartBeat(msg.src.ToU64());
      }
    }

  end:
    tracer.PrepareState1();
    tracer.Finish();
  }
  return 0;
}

}  // namespace vraft
