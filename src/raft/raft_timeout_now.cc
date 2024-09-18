#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "clock.h"
#include "raft.h"
#include "raft_server.h"
#include "util.h"
#include "vraft_logger.h"

namespace vraft {

int32_t Raft::OnTimeoutNow(struct TimeoutNow &msg) {
  if (started_) {
    leader_transfer_ = true;
    transfer_max_term_ = meta_.term() + MAX_TRANSFER_TERM;

    Tracer tracer(this, true, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    RaftIndex last_index = LastIndex();
    RaftTerm last_term = LastTerm();
    bool my_log_enough =
        ((msg.last_log_term <= last_term) ||
         (msg.last_log_term == last_term && msg.last_log_index <= last_index));

    // maybe step down first
    if (msg.term > meta_.term()) {
      StepDown(msg.term, &tracer);
    }
    assert(msg.term <= meta_.term());

    if (!msg.force && !my_log_enough) {
      ;
    } else {
      if (state_ == STATE_FOLLOWER) {
        timer_mgr_.StartElection();
      }
    }

    tracer.PrepareState1();
    tracer.Finish();
  }
  return 0;
}

int32_t Raft::SendTimeoutNow(uint64_t dest, bool force, Tracer *tracer) {
  TimeoutNow msg;
  msg.src = Me();
  msg.dest = RaftAddr(dest);
  msg.term = meta_.term();
  msg.uid = UniqId(&msg);
  msg.send_ts = Clock::NSec();
  msg.elapse = 0;

  msg.last_log_index = LastIndex();
  msg.last_log_term = LastTerm();
  msg.force = force;

  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kTimeoutNow;
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

}  // namespace vraft
