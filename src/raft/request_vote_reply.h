#ifndef VRAFT_REQUEST_VOTE_REPLY_H_
#define VRAFT_REQUEST_VOTE_REPLY_H_

#include <stdint.h>

#include "allocator.h"
#include "common.h"
#include "message.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "util.h"

namespace vraft {

struct RequestVoteReply : public Message {
  RaftAddr src;   // uint64_t
  RaftAddr dest;  // uint64_t
  RaftTerm term;
  uint32_t uid;
  uint64_t send_ts;  // nanosecond
  uint64_t elapse;   // microsecond

  bool granted;      // uint8_t
  bool log_ok;       // uint8_t
  bool pre_vote;     // uint8_t
  bool interval_ok;  // uint8_t

  // send back
  RaftTerm req_term;

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
