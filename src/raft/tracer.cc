#include "tracer.h"

#include "raft.h"
#include "util.h"
#include "vraft_logger.h"

namespace vraft {

void Tracer::Init() {
  ts_ = Clock::NSec();
  snprintf(ts_buf_, sizeof(ts_buf_), "0x%lX", ts_);

  state_header_.clear();
  state0_.clear();
  state1_.clear();
  events_.clear();
}

std::string Tracer::TimeStampStr() {
  if (timestamp_) {
    return " " + Clock::NSecStr();
  } else {
    return "";
  }
}

void Tracer::PrepareEvent(EventType event_type, std::string s) {
  if (enable_) {
    switch (event_type) {
      case kEventStart: {
        events_.push_back(std::string(ts_buf_) + TimeStampStr() +
                          " event_start : " + s);
        break;
      }

      case kEventStop: {
        events_.push_back(std::string(ts_buf_) + TimeStampStr() +
                          " event_stop  : " + s);
        break;
      }

      case kEventSend: {
        events_.push_back(std::string(ts_buf_) + TimeStampStr() +
                          " event_send  : " + s);
        break;
      }
      case kEventRecv: {
        events_.push_back(std::string(ts_buf_) + TimeStampStr() +
                          " event_recv  : " + s);
        break;
      }
      case kEventTimer: {
        events_.push_back(std::string(ts_buf_) + TimeStampStr() +
                          " event_timer : " + s);
        break;
      }
      case kEventOther: {
        events_.push_back(std::string(ts_buf_) + TimeStampStr() +
                          " event_other : " + s);
        break;
      }
      default:
        assert(0);
    }
  }
}

void Tracer::PrepareState0() {
  if (enable_) {
    state_header_ = std::string(ts_buf_) + " state_change: " + NsToString(ts_) +
                    " --- " + raft_->Me().ToString() + " ---";
    state0_ = std::string(ts_buf_) + TimeStampStr() +
              " state_begin : " + raft_->ToJsonString(true, true);
  }
}

void Tracer::PrepareState1() {
  if (enable_) {
    state1_ = std::string(ts_buf_) + TimeStampStr() +
              " state_end   : " + raft_->ToJsonString(true, true);
  }
}

std::string Tracer::Finish() {
  std::string s;
  if (enable_) {
    s.append("\n" + state_header_);
    s.append("\n" + state0_);
    for (auto &e : events_) {
      s.append("\n" + e);
    }
    s.append("\n" + state1_);
  }

  if (enable_) {
    vraft_logger.Trace("%s", s.c_str());

    if (cb_) {
      cb_(std::string(ts_buf_));
    }
  }

  return s;
}
}  // namespace vraft
