#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "clock.h"
#include "raft.h"
#include "raft_server.h"
#include "util.h"
#include "vraft_logger.h"

namespace vraft {

int32_t Raft::OnInstallSnapshot(struct InstallSnapshot &msg) {
  if (started_) {
    Tracer tracer(this, true, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    InstallSnapshotReply reply;
    reply.src = msg.dest;
    reply.dest = msg.src;
    reply.term = meta_.term();
    reply.uid = UniqId(&reply);
    reply.send_ts = Clock::NSec();
    reply.elapse = 0;
    reply.stored = 0;

    // temp variable used behind, define here due to "goto", make compiler happy
    bool match = false;
    SnapshotWriterSPtr writer;
    int32_t rv = 0;

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

    // reset election timer immediately
    timer_mgr_.StopRequestVote();
    timer_mgr_.AgainElection();

    // record last_heartbeat_timestamp
    last_heartbeat_timestamp_ = Clock::NSec();

    // update leader cache
    if (leader_.ToU64() == 0) {
      leader_ = msg.src;
    } else {
      assert(leader_.ToU64() == msg.src.ToU64());
    }

    // create snapshot writer
    if (snapshot_mgr_.snapshots[msg.src.ToU64()].writer_) {
      match = (snapshot_mgr_.snapshots[msg.src.ToU64()].writer_->last_index() ==
               msg.last_index) &&
              (snapshot_mgr_.snapshots[msg.src.ToU64()].writer_->last_term() ==
               msg.last_term);

      if (!match) {
        snapshot_mgr_.snapshots[msg.src.ToU64()].writer_ = nullptr;
      }
    }

    if (!snapshot_mgr_.snapshots[msg.src.ToU64()].writer_ && create_writer_) {
      std::string path = sm_path_ + "-snapshot-" + NsToString2(Clock::NSec());
      snapshot_mgr_.snapshots[msg.src.ToU64()].writer_ =
          create_writer_(path, msg.last_index, msg.last_term);
    }

    writer = snapshot_mgr_.snapshots[msg.src.ToU64()].writer_;
    assert(writer);

    // rewrite stored
    reply.stored = writer->stored();

    // already have written data
    if (msg.offset < writer->stored()) {
      goto end;
    }
    assert(msg.offset > writer->stored());

    rv = writer->Write(msg.data);
    assert(rv == 0);

    // rewrite stored
    reply.stored = writer->stored();

    // reset election timer again
    timer_mgr_.StopRequestVote();
    timer_mgr_.AgainElection();

    // done
    if (msg.done) {
      // stale snapshot, discard
      if (sm_ && msg.last_index < sm_->LastIndex()) {
        snapshot_mgr_.snapshots[msg.src.ToU64()].writer_ = nullptr;
        goto end;
      }

      // finish ok
      rv = writer->Finish();
      assert(rv == 0);

      // update state machine
      sm_.reset();
      std::string delete_path =
          sm_path_ + "-delete-" + NsToString2(Clock::NSec());
      AtomicMove(sm_path_, delete_path);
      AtomicMove(writer->path(), sm_path_);
      if (create_sm_) {
        sm_ = create_sm_(sm_path_);
        assert(sm_);
      }

      snapshot_mgr_.snapshots[msg.src.ToU64()].writer_ = nullptr;
    }

  end:
    SendInstallSnapshotReply(reply, &tracer);
    tracer.PrepareState1();
    tracer.Finish();
  }

  return 0;
}

int32_t Raft::OnInstallSnapshotReply(struct InstallSnapshotReply &msg) {
  if (started_) {
    Tracer tracer(this, true, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    SnapshotReaderSPtr reader =
        snapshot_mgr_.snapshots[msg.src.ToU64()].reader_;
    assert(reader);

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

    assert((state_ == STATE_LEADER));

    if (msg.term > meta_.term()) {
      StepDown(msg.term, &tracer);

    } else {
      assert(msg.term == meta_.term());
      assert(msg.req_term == meta_.term());

      // done
      if (msg.stored == reader->offset() - 1 && reader->done()) {
        index_mgr_.SetMatch(msg.src, reader->last_index());
        index_mgr_.SetNext(msg.src, index_mgr_.GetMatch(msg.src) + 1);

        MaybeCommit(&tracer);

        reader->Finish();
        snapshot_mgr_.snapshots[msg.src.ToU64()].reader_ = nullptr;
      }

      // send msg immediately
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

int32_t Raft::SendInstallSnapshotReply(InstallSnapshotReply &msg,
                                       Tracer *tracer) {
  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kInstallSnapshotReply;
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

int32_t Raft::SendInstallSnapshot(uint64_t dest, Tracer *tracer) {
  InstallSnapshot msg;
  msg.src = Me();
  msg.dest = RaftAddr(dest);
  msg.term = meta_.term();
  msg.uid = UniqId(&msg);
  msg.send_ts = Clock::NSec();
  msg.elapse = 0;

  if (!snapshot_mgr_.snapshots[dest].reader_ && create_reader_) {
    snapshot_mgr_.snapshots[dest].reader_ =
        create_reader_(sm_path_, SNAPSHOT_MAX_READ);
  }
  SnapshotReaderSPtr reader = snapshot_mgr_.snapshots[dest].reader_;
  assert(reader);

  msg.last_index = reader->last_index();
  msg.last_term = reader->last_term();
  msg.offset = reader->offset();
  msg.done = false;

  int32_t rv = reader->Read();
  if (rv == 0) {
    msg.done = true;
  } else {
    assert(rv > 0);
  }
  msg.data = reader->data();

  std::string body_str;
  int32_t bytes = msg.ToString(body_str);

  MsgHeader header;
  header.body_bytes = bytes;
  header.type = kInstallSnapshot;
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
