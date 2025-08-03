#ifndef VRAFT_CLIENT_REQUEST_REPLY_H_
#define VRAFT_CLIENT_REQUEST_REPLY_H_

#include <string>

#include "allocator.h"
#include "common.h"
#include "message.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "util.h"

namespace vraft {

struct ClientRequestReply : public Message {
  uint32_t uid;
  int32_t code;
  std::string msg;
  std::string data;

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
