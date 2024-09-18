#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "clock.h"
#include "raft.h"
#include "raft_server.h"
#include "util.h"
#include "vraft_logger.h"
#include "vstore_sm.h"

namespace vraft {

int32_t Raft::OnClientRequest(struct ClientRequest &msg,
                              vraft::TcpConnectionSPtr conn) {
  int32_t rv = 0;
  if (started_) {
    Tracer tracer(this, true, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    switch (msg.cmd) {
      case kCmdPropose: {
        rv = Propose(msg.data, nullptr);
        break;
      }

      case kCmdLeaderTransfer: {
        RaftAddr dest;
        bool b = dest.FromString(msg.data);
        if (b) {
          rv = LeaderTransfer(dest);
        } else {
          rv = -1;
        }
        break;
      }

      case kCmdAddServer: {
        break;
      }

      case kCmdRemoveServer: {
        break;
      }

      case kCmdGet: {
        vstore::VstoreSm *sm = reinterpret_cast<vstore::VstoreSm *>(sm_.get());
        assert(sm);
        std::string value;
        rv = sm->Get(msg.data, value);

        {
          // reply
          if (rv == -2) {
            value = "not found";
          } else if (rv == -1) {
            value = "error";
          }

          conn->CopySend(value.c_str(), value.size());
        }

        break;
      }

      default:
        break;
    }

    tracer.PrepareState1();
    tracer.Finish();
  }

  return rv;
}

int32_t Raft::DoPropose(const std::string &value, EntryType type,
                        Tracer *tracer) {
  AppendEntry entry;
  entry.term = meta_.term();
  entry.type = type;
  entry.value = value;
  int32_t rv = log_.AppendOne(entry, tracer);
  assert(rv == 0);

  MaybeCommit(tracer);
  if (config_mgr_.Current()->peers.size() > 0) {
    for (auto &peer : config_mgr_.Current()->peers) {
      rv = SendAppendEntries(peer.ToU64(), tracer);
      assert(rv == 0);

      timer_mgr_.AgainHeartBeat(peer.ToU64());
    }
  }

  return rv;
}

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
  snprintf(buf, sizeof(buf), "%s propose-value length:%lu",
           Me().ToString().c_str(), value.size());
  tracer.PrepareEvent(kEventOther, std::string(buf));

  int32_t rv = 0;
  if (state_ != STATE_LEADER || !started_) {
    rv = -1;
    goto end;
  }

  rv = DoPropose(value, kData, &tracer);

end:
  tracer.PrepareState1();
  tracer.Finish();
  return rv;
}

int32_t Raft::LeaderTransfer(RaftAddr &dest) {
  if (assert_loop_) {
    assert_loop_();
  }

  Tracer tracer(this, true, tracer_cb_);
  tracer.PrepareState0();
  char buf[128];
  snprintf(buf, sizeof(buf), "%s leader-transfer to:%s",
           Me().ToString().c_str(), dest.ToString().c_str());
  tracer.PrepareEvent(kEventOther, std::string(buf));

  int32_t rv = 0;
  if (state_ != STATE_LEADER || !started_) {
    vraft_logger.Error("%s leader transfer error, not leader",
                       Me().ToString().c_str());
    rv = -1;
    goto end;
  }

  rv = SendTimeoutNow(dest.ToU64(), false, &tracer);

end:
  tracer.PrepareState1();
  tracer.Finish();
  return rv;
}

int32_t Raft::LeaderTransferFirstPeer() {
  std::vector<RaftAddr> peers = Peers();
  if (peers.size() == 0) {
    return -1;
  }

  return LeaderTransfer(peers[0]);
}

int32_t Raft::AddServer(const RaftAddr &addr) {
  if (assert_loop_) {
    assert_loop_();
  }

  if (changing_index_ > 0) {
    return -1;
  }

  bool in = config_mgr_.Current()->InConfig(addr);
  if (in) {
    return -1;
  }

  RaftConfig rc = *(config_mgr_.Current());
  rc.peers.push_back(addr);

  std::string value;
  rc.ToString(value);

  //---------------------- propose value

  Tracer tracer(this, true, tracer_cb_);
  tracer.PrepareState0();
  char buf[128];
  snprintf(buf, sizeof(buf), "%s config-change-propose %s",
           Me().ToString().c_str(), rc.ToJsonString(true, true).c_str());
  tracer.PrepareEvent(kEventOther, std::string(buf));

  int32_t rv = 0;
  if (state_ != STATE_LEADER || !started_) {
    rv = -1;
    goto end;
  }

  rv = DoPropose(value, kConfig, &tracer);

end:
  tracer.PrepareState1();
  tracer.Finish();
  return rv;
}

int32_t Raft::RemoveServer(const RaftAddr &addr) {
  if (assert_loop_) {
    assert_loop_();
  }

  if (changing_index_ > 0) {
    return -1;
  }

  bool in = config_mgr_.Current()->InConfig(addr);
  if (!in) {
    return -1;
  }

  if (config_mgr_.Current()->me == addr) {
    return -1;
  }

  RaftConfig rc = *(config_mgr_.Current());
  rc.peers.erase(std::remove(rc.peers.begin(), rc.peers.end(), addr),
                 rc.peers.end());

  std::string value;
  rc.ToString(value);

  //---------------------- propose value

  Tracer tracer(this, true, tracer_cb_);
  tracer.PrepareState0();
  char buf[128];
  snprintf(buf, sizeof(buf), "%s config-change-propose %s",
           Me().ToString().c_str(), rc.ToJsonString(true, true).c_str());
  tracer.PrepareEvent(kEventOther, std::string(buf));

  int32_t rv = 0;
  if (state_ != STATE_LEADER || !started_) {
    rv = -1;
    goto end;
  }

  rv = DoPropose(value, kConfig, &tracer);

end:
  tracer.PrepareState1();
  tracer.Finish();
  return rv;
}

}  // namespace vraft
