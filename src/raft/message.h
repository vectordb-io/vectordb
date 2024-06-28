#ifndef VRAFT_MESSAGE_H_
#define VRAFT_MESSAGE_H_

#include <cstdint>
#include <string>

#include "allocator.h"
#include "coding.h"
#include "nlohmann/json.hpp"

namespace vraft {

enum MsgType {
  kPing = 0,
  kPingReply,
  kRequestVote,
  kRequestVoteReply,
  kAppendEntries,
  kAppendEntriesReply,
  kInstallSnapshot,
  kInstallSnapshotReply,
  kMsgNum,
};

struct MsgHeader {
  int32_t body_bytes;
  int32_t type;

  int32_t Bytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  bool FromString(std::string &s);
  bool FromString(const char *ptr, int32_t len);
};

inline int32_t MsgHeader::Bytes() { return sizeof(MsgHeader); }

inline int32_t MsgHeader::ToString(std::string &s) {
  s.clear();
  s.reserve(Bytes());
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(Bytes()));
  int32_t size = ToString(ptr, Bytes());
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

inline int32_t MsgHeader::ToString(const char *ptr, int32_t len) {
  assert(len >= Bytes());
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  EncodeFixed32(p, body_bytes);
  p += sizeof(body_bytes);
  size += sizeof(body_bytes);

  EncodeFixed32(p, type);
  p += sizeof(type);
  size += sizeof(type);

  return size;
}

inline bool MsgHeader::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

inline bool MsgHeader::FromString(const char *ptr, int32_t len) {
  assert(len == Bytes());
  char *p = const_cast<char *>(ptr);

  body_bytes = DecodeFixed32(p);
  p += sizeof(body_bytes);

  type = DecodeFixed32(p);
  p += sizeof(type);

  return true;
}

struct Message {
  virtual int32_t MaxBytes() = 0;
  virtual int32_t ToString(std::string &s) = 0;
  virtual int32_t ToString(const char *ptr, int32_t len) = 0;
  virtual int32_t FromString(std::string &s) = 0;
  virtual int32_t FromString(const char *ptr, int32_t len) = 0;

  virtual nlohmann::json ToJson() = 0;
  virtual nlohmann::json ToJsonTiny() = 0;
  virtual std::string ToJsonString(bool tiny, bool one_line) = 0;
};

}  // namespace vraft

#endif
