#ifndef VRAFT_INSTALL_SNAPSHOT_H_
#define VRAFT_INSTALL_SNAPSHOT_H_

#include <vector>

#include "allocator.h"
#include "common.h"
#include "kv.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "raft_log.h"
#include "util.h"

namespace vraft {

struct InstallSnapshot {
  RaftAddr src;   // uint64_t
  RaftAddr dest;  // uint64_t
  RaftTerm term;
  uint32_t uid;
  uint64_t send_ts;  // nanosecond
  uint64_t elapse;   // microsecond

  RaftIndex last_index;
  RaftTerm last_term;

  int32_t offset;
  std::string data;
  bool done;  // uint8_t

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

inline int32_t InstallSnapshot::MaxBytes() {
  int32_t size = 0;
  size += sizeof(uint64_t);
  size += sizeof(uint64_t);
  size += sizeof(term);
  size += sizeof(uid);
  size += sizeof(send_ts);
  size += sizeof(elapse);
  size += sizeof(last_index);
  size += sizeof(last_term);
  size += sizeof(offset);
  size += 2 * sizeof(int32_t) + data.size();
  size += sizeof(uint8_t);
  return size;
}

inline int32_t InstallSnapshot::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

inline int32_t InstallSnapshot::ToString(const char *ptr, int32_t len) {
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

  EncodeFixed32(p, last_index);
  p += sizeof(last_index);
  size += sizeof(last_index);

  EncodeFixed64(p, last_term);
  p += sizeof(last_term);
  size += sizeof(last_term);

  EncodeFixed32(p, offset);
  p += sizeof(offset);
  size += sizeof(offset);

  {
    Slice sls(data.c_str(), data.size());
    char *p2 = EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  uint8_t u8 = done;
  EncodeFixed8(p, u8);
  p += sizeof(u8);
  size += sizeof(u8);

  assert(size <= len);
  return size;
}

inline int32_t InstallSnapshot::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

inline int32_t InstallSnapshot::FromString(const char *ptr, int32_t len) {
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

  last_index = DecodeFixed32(p);
  p += sizeof(last_index);
  size += sizeof(last_index);

  last_term = DecodeFixed64(p);
  p += sizeof(last_term);
  size += sizeof(last_term);

  offset = DecodeFixed32(p);
  p += sizeof(offset);
  size += sizeof(offset);

  {
    Slice result;
    Slice input(p, len - size);
    int32_t sz = DecodeString2(&input, &result);
    if (sz > 0) {
      data.clear();
      data.append(result.data(), result.size());
      size += sz;
      p += sz;
    }
  }

  done = DecodeFixed8(p);
  p += sizeof(done);
  size += sizeof(done);

  return size;
}

inline nlohmann::json InstallSnapshot::ToJson() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dest"] = dest.ToString();
  j[0]["term"] = term;
  j[0]["uid"] = U32ToHexStr(uid);

  j[1]["last_index"] = last_index;
  j[1]["last_term"] = last_term;
  j[1]["offset"] = offset;
  j[2]["data_size"] = data.size();

  j[3]["done"] = done;

  j[4]["send_ts"] = send_ts;
  j[4]["elapse"] = elapse;

  return j;
}

inline nlohmann::json InstallSnapshot::ToJsonTiny() { return ToJson(); }

inline std::string InstallSnapshot::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["install_snapshot"] = ToJsonTiny();
  } else {
    j["install_snapshot"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
