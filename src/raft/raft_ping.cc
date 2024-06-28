#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "clock.h"
#include "raft.h"
#include "raft_server.h"
#include "util.h"
#include "vraft_logger.h"

namespace vraft {

int32_t Raft::OnPing(struct Ping &msg) {
  if (started_) {
    vraft_logger.Info("%s recv ping from %s, msg:%s",
                      msg.dest.ToString().c_str(), msg.src.ToString().c_str(),
                      msg.msg.c_str());

    Tracer tracer(this, false, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    PingReply reply;
    reply.src = msg.dest;
    reply.dest = msg.src;
    reply.uid = UniqId(&reply);
    reply.msg = "pang";
    std::string reply_str;
    int32_t bytes = reply.ToString(reply_str);

    MsgHeader header;
    header.body_bytes = bytes;
    header.type = kPingReply;
    std::string header_str;
    header.ToString(header_str);

    if (send_) {
      header_str.append(std::move(reply_str));
      send_(reply.dest.ToU64(), header_str.data(), header_str.size());
      tracer.PrepareEvent(kEventSend, reply.ToJsonString(false, true));
    }

    tracer.PrepareState1();
    tracer.Finish();
  }

  return 0;
}

int32_t Raft::OnPingReply(struct PingReply &msg) {
  if (started_) {
    Tracer tracer(this, false, tracer_cb_);
    tracer.PrepareState0();
    tracer.PrepareEvent(kEventRecv, msg.ToJsonString(false, true));

    vraft_logger.Info("%s recv ping-reply from %s, msg:%s",
                      msg.dest.ToString().c_str(), msg.src.ToString().c_str(),
                      msg.msg.c_str());

    tracer.PrepareState1();
    tracer.Finish();
  }
  return 0;
}

}  // namespace vraft
