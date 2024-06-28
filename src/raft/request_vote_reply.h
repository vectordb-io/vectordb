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

  bool granted;  // uint8_t

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

inline int32_t RequestVoteReply::MaxBytes() {
  return sizeof(uint64_t) + sizeof(uint64_t) + sizeof(term) + sizeof(uid) +
         sizeof(uint8_t) + sizeof(req_term);
}

inline int32_t RequestVoteReply::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

inline int32_t RequestVoteReply::ToString(const char *ptr, int32_t len) {
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

  EncodeFixed8(p, granted);
  p += sizeof(granted);
  size += sizeof(uint8_t);

  EncodeFixed64(p, req_term);
  p += sizeof(req_term);
  size += sizeof(req_term);

  assert(size <= len);
  return size;
}

inline int32_t RequestVoteReply::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

inline int32_t RequestVoteReply::FromString(const char *ptr, int32_t len) {
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

  granted = DecodeFixed8(p);
  p += sizeof(uint8_t);
  size += sizeof(uint8_t);

  req_term = DecodeFixed64(p);
  p += sizeof(req_term);
  size += sizeof(req_term);

  return size;
}

inline nlohmann::json RequestVoteReply::ToJson() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dest"] = dest.ToString();
  j[0]["term"] = term;
  j[0]["uid"] = U32ToHexStr(uid);
  j[1]["grant"] = granted;
  j[1]["req-term"] = req_term;
  return j;
}

inline nlohmann::json RequestVoteReply::ToJsonTiny() {
  nlohmann::json j;
  j["src"] = src.ToString();
  j["dst"] = dest.ToString();
  j["tm"] = term;
  j["gr"] = granted;
  j["rtm"] = req_term;
  j["uid"] = U32ToHexStr(uid);
  return j;
}

inline std::string RequestVoteReply::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["rvr"] = ToJsonTiny();
  } else {
    j["request-vote-reply"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
