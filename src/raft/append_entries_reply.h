#ifndef VRAFT_APPEND_ENTRIES_REPLY_H_
#define VRAFT_APPEND_ENTRIES_REPLY_H_

#include <stdint.h>

#include "allocator.h"
#include "common.h"
#include "message.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "util.h"

namespace vraft {

struct AppendEntriesReply : public Message {
  RaftAddr src;   // uint64_t
  RaftAddr dest;  // uint64_t
  RaftTerm term;
  uint32_t uid;

  bool success;              // uint8_t
  RaftIndex last_log_index;  // to speed up

  // send back
  RaftIndex req_pre_index;  // from leader
  int32_t req_num_entries;  // from leader
  RaftTerm req_term;        // from leader

  int32_t MaxBytes() override;
  int32_t ToString(std::string &s) override;
  int32_t ToString(const char *ptr, int32_t len) override;
  int32_t FromString(std::string &s) override;
  int32_t FromString(const char *ptr, int32_t len) override;

  nlohmann::json ToJson() override;
  nlohmann::json ToJsonTiny() override;
  std::string ToJsonString(bool tiny, bool one_line) override;
};

inline int32_t AppendEntriesReply::MaxBytes() {
  return sizeof(uint64_t) + sizeof(uint64_t) + sizeof(term) + sizeof(uid) +
         sizeof(uint8_t) + sizeof(last_log_index) + sizeof(req_pre_index) +
         sizeof(req_num_entries) + sizeof(req_term);
}

inline int32_t AppendEntriesReply::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

inline int32_t AppendEntriesReply::ToString(const char *ptr, int32_t len) {
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

  EncodeFixed8(p, success);
  p += sizeof(success);
  size += sizeof(uint8_t);

  EncodeFixed32(p, last_log_index);
  p += sizeof(last_log_index);
  size += sizeof(last_log_index);

  EncodeFixed32(p, req_pre_index);
  p += sizeof(req_pre_index);
  size += sizeof(req_pre_index);

  EncodeFixed32(p, req_num_entries);
  p += sizeof(req_num_entries);
  size += sizeof(req_num_entries);

  EncodeFixed64(p, req_term);
  p += sizeof(req_term);
  size += sizeof(req_term);

  assert(size <= len);
  return size;
}

inline int32_t AppendEntriesReply::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

inline int32_t AppendEntriesReply::FromString(const char *ptr, int32_t len) {
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

  success = DecodeFixed8(p);
  p += sizeof(uint8_t);
  size += sizeof(uint8_t);

  last_log_index = DecodeFixed32(p);
  p += sizeof(last_log_index);
  size += sizeof(last_log_index);

  req_pre_index = DecodeFixed32(p);
  p += sizeof(req_pre_index);
  size += sizeof(req_pre_index);

  req_num_entries = DecodeFixed32(p);
  p += sizeof(req_num_entries);
  size += sizeof(req_num_entries);

  req_term = DecodeFixed64(p);
  p += sizeof(req_term);
  size += sizeof(req_term);

  return size;
}

inline nlohmann::json AppendEntriesReply::ToJson() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dest"] = dest.ToString();
  j[0]["term"] = term;
  j[0]["uid"] = U32ToHexStr(uid);
  j[1]["success"] = success;
  j[1]["last"] = last_log_index;
  j[2]["req-pre"] = req_pre_index;
  j[2]["req-entry-count"] = req_num_entries;
  j[2]["req-term"] = req_term;
  return j;
}

inline nlohmann::json AppendEntriesReply::ToJsonTiny() {
  nlohmann::json j;
  j["src"] = src.ToString();
  j["dst"] = dest.ToString();
  j["tm"] = term;
  j["uid"] = U32ToHexStr(uid);
  j["suc"] = success;
  j["last"] = last_log_index;
  j["req-pre"] = req_pre_index;
  j["req-cnt"] = req_num_entries;
  j["req-tm"] = req_term;
  return j;
}

inline std::string AppendEntriesReply::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["aer"] = ToJsonTiny();
  } else {
    j["append-entries-reply"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
