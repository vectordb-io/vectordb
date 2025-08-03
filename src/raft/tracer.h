#ifndef VRAFT_TRACER_H_
#define VRAFT_TRACER_H_

#include <string>
#include <vector>

#include "clock.h"
#include "common.h"

namespace vraft {

class Raft;

enum EventType {
  kEventSend = 0,
  kEventStart,
  kEventStop,
  kEventRecv,
  kEventTimer,
  kEventOther,
  kEventTypeNum,
};

class Tracer final {
 public:
  Tracer(Raft *r, bool enable, TracerCb cb);
  ~Tracer();
  Tracer(const Tracer &t) = delete;
  Tracer &operator=(const Tracer &t) = delete;

  bool enable() { return enable_; }
  void Enable() { enable_ = true; }
  void Disable() { enable_ = false; }

  void EnableTimeStamp() { timestamp_ = true; }
  void DisableTimeStamp() { timestamp_ = false; }

  void PrepareState0();
  void PrepareState1();
  void PrepareEvent(EventType event_type, std::string s);
  std::string Finish();

 private:
  void Init();
  std::string TimeStampStr();

 private:
  bool enable_;
  bool timestamp_;

  Raft *raft_;
  uint64_t ts_;
  char ts_buf_[32];
  std::string state_header_;
  std::string state0_;
  std::string state1_;
  std::vector<std::string> events_;

  TracerCb cb_;
};

inline Tracer::Tracer(Raft *r, bool enable, TracerCb cb)
    : enable_(enable), timestamp_(false), raft_(r), cb_(cb) {
  Init();
}

inline Tracer::~Tracer() {}

}  // namespace vraft

#endif
