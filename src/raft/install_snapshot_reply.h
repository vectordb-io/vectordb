#ifndef VRAFT_INSTALL_SNAPSHOT_REPLY_H_
#define VRAFT_INSTALL_SNAPSHOT_REPLY_H_

#include <stdint.h>

#include "allocator.h"
#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "util.h"

namespace vraft {

struct InstallSnapshotReply {
  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  bool FromString(std::string &s);
  bool FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

inline int32_t InstallSnapshotReply::MaxBytes() { return 0; }
inline int32_t InstallSnapshotReply::ToString(std::string &s) { return 0; }
inline int32_t InstallSnapshotReply::ToString(const char *ptr, int32_t len) {
  return 0;
}
inline bool InstallSnapshotReply::FromString(std::string &s) { return 0; }
inline bool InstallSnapshotReply::FromString(const char *ptr, int32_t len) {
  return 0;
}

inline nlohmann::json InstallSnapshotReply::ToJson() { return 0; }
inline nlohmann::json InstallSnapshotReply::ToJsonTiny() { return 0; }
inline std::string InstallSnapshotReply::ToJsonString(bool tiny,
                                                      bool one_line) {
  return 0;
}

}  // namespace vraft

#endif
