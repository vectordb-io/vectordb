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
  RaftAddr src;   // uint64_t
  RaftAddr dest;  // uint64_t
  RaftTerm term;
  uint32_t uid;
  uint64_t send_ts;  // nanosecond
  uint64_t elapse;   // microsecond

  // maybe count, maybe bytes, defined by user
  int32_t stored;

  // send back
  RaftTerm req_term;

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  bool FromString(std::string &s);
  bool FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

inline int32_t InstallSnapshotReply::MaxBytes() {
  int32_t size = 0;
  size += sizeof(uint64_t);
  size += sizeof(uint64_t);
  size += sizeof(term);
  size += sizeof(uid);
  size += sizeof(send_ts);
  size += sizeof(elapse);
  size += sizeof(stored);
  size += sizeof(req_term);
  return size;
}

inline int32_t InstallSnapshotReply::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

inline int32_t InstallSnapshotReply::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;
  uint64_t u64 = 0;

  u64 = src.ToU64();
  EncodeFixed64(p, u64);
  p += sizeof(u64);
  size += sizeof(u64);

  u64 = dest.ToU64();
  EncodeFixed64(p, u64);
  p += sizeof(u64);
  size += sizeof(u64);

  EncodeFixed64(p, term);
  p += sizeof(term);
  size += sizeof(term);

  EncodeFixed32(p, uid);
  p += sizeof(uid);
  size += sizeof(uid);

  EncodeFixed64(p, send_ts);
  p += sizeof(send_ts);
  size += sizeof(send_ts);

  EncodeFixed64(p, elapse);
  p += sizeof(elapse);
  size += sizeof(elapse);

  EncodeFixed32(p, stored);
  p += sizeof(stored);
  size += sizeof(stored);

  EncodeFixed64(p, req_term);
  p += sizeof(req_term);
  size += sizeof(req_term);

  assert(size <= len);
  return size;
}

inline bool InstallSnapshotReply::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

inline bool InstallSnapshotReply::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  uint64_t u64 = 0;
  int32_t size = 0;

  u64 = DecodeFixed64(p);
  src.FromU64(u64);
  p += sizeof(u64);
  size += sizeof(u64);

  u64 = DecodeFixed64(p);
  dest.FromU64(u64);
  p += sizeof(u64);
  size += sizeof(u64);

  term = DecodeFixed64(p);
  p += sizeof(term);
  size += sizeof(term);

  uid = DecodeFixed32(p);
  p += sizeof(uid);
  size += sizeof(uid);

  send_ts = DecodeFixed64(p);
  p += sizeof(send_ts);
  size += sizeof(send_ts);

  elapse = DecodeFixed64(p);
  p += sizeof(elapse);
  size += sizeof(elapse);

  stored = DecodeFixed32(p);
  p += sizeof(stored);
  size += sizeof(stored);

  req_term = DecodeFixed64(p);
  p += sizeof(req_term);
  size += sizeof(req_term);

  return size;
}

inline nlohmann::json InstallSnapshotReply::ToJson() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dest"] = dest.ToString();
  j[0]["term"] = term;
  j[0]["uid"] = U32ToHexStr(uid);
  j[1]["stored"] = stored;
  j[1]["req_term"] = req_term;
  j[2]["send_ts"] = send_ts;
  j[2]["elapse"] = elapse;
  return j;
}

inline nlohmann::json InstallSnapshotReply::ToJsonTiny() { return ToJson(); }

inline std::string InstallSnapshotReply::ToJsonString(bool tiny,
                                                      bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["install_snapshot_reply"] = ToJsonTiny();
  } else {
    j["install_snapshot_reply"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
