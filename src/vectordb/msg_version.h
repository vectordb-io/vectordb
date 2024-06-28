#ifndef VECTORDB_MSG_VERSION_H_
#define VECTORDB_MSG_VERSION_H_

#include <memory>

#include "allocator.h"
#include "common.h"
#include "message.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"

namespace vectordb {

class MsgVersion;
using MsgVersionSPtr = std::shared_ptr<MsgVersion>;
using MsgVersionUPtr = std::unique_ptr<MsgVersion>;
using MsgVersionWPtr = std::weak_ptr<MsgVersion>;

struct MsgVersion : public vraft::Message {
  uint64_t seqid;

  int32_t MaxBytes() override;
  int32_t ToString(std::string &s) override;
  int32_t ToString(const char *ptr, int32_t len) override;
  int32_t FromString(std::string &s) override;
  int32_t FromString(const char *ptr, int32_t len) override;

  nlohmann::json ToJson() override;
  nlohmann::json ToJsonTiny() override;
  std::string ToJsonString(bool tiny, bool one_line) override;
};

}  // namespace vectordb

#endif
