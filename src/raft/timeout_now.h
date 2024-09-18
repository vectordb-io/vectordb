#ifndef VRAFT_TIMEOUT_NOW_H_
#define VRAFT_TIMEOUT_NOW_H_

#include <stdint.h>

#include "allocator.h"
#include "common.h"
#include "message.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "util.h"

namespace vraft {

struct TimeoutNow : public Message {
  RaftAddr src;   // uint64_t
  RaftAddr dest;  // uint64_t
  RaftTerm term;
  uint32_t uid;
  uint64_t send_ts;  // nanosecond
  uint64_t elapse;   // microsecond

  RaftIndex last_log_index;
  RaftTerm last_log_term;
  bool force;  // uint8_t

  int32_t MaxBytes() override;
  int32_t ToString(std::string &s) override;
  int32_t ToString(const char *ptr, int32_t len) override;
  int32_t FromString(std::string &s) override;
  int32_t FromString(const char *ptr, int32_t len) override;

  nlohmann::json ToJson() override;
  nlohmann::json ToJsonTiny() override;
  std::string ToJsonString(bool tiny, bool one_line) override;
};

}  // namespace vraft

#endif
