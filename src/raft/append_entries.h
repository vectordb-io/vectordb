#ifndef VRAFT_APPEND_ENTRIES_H_
#define VRAFT_APPEND_ENTRIES_H_

#include <stdint.h>

#include <vector>

#include "allocator.h"
#include "common.h"
#include "message.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "raft_log.h"
#include "util.h"

namespace vraft {

struct AppendEntries : public Message {
  RaftAddr src;   // uint64_t
  RaftAddr dest;  // uint64_t
  RaftTerm term;
  uint32_t uid;

  RaftIndex pre_log_index;
  RaftTerm pre_log_term;
  RaftIndex commit_index;
  std::vector<LogEntry> entries;

  int32_t MaxBytes() override;
  int32_t ToString(std::string &s) override;
  int32_t ToString(const char *ptr, int32_t len) override;
  int32_t FromString(std::string &s) override;
  int32_t FromString(const char *ptr, int32_t len) override;

  nlohmann::json ToJson() override;
  nlohmann::json ToJsonTiny() override;
  std::string ToJsonString(bool tiny, bool one_line) override;
};

inline int32_t AppendEntries::MaxBytes() {
  int32_t size = sizeof(uint64_t) + sizeof(uint64_t) + sizeof(term) +
                 sizeof(uid) + sizeof(pre_log_index) + sizeof(pre_log_term) +
                 sizeof(commit_index) + 2 * sizeof(int32_t);
  for (auto &e : entries) {
    size += sizeof(e.index);
    size += sizeof(e.pre_chk_all);
    size += sizeof(e.chk_ths);
    size += sizeof(e.chk_all);
    size += sizeof(e.append_entry.term);
    size += sizeof(e.append_entry.type);
    size += 2 * sizeof(int32_t);
    size += e.append_entry.value.size();
  }
  return size;
}

inline int32_t AppendEntries::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

inline int32_t AppendEntries::ToString(const char *ptr, int32_t len) {
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

  EncodeFixed32(p, pre_log_index);
  p += sizeof(pre_log_index);
  size += sizeof(pre_log_index);

  EncodeFixed64(p, pre_log_term);
  p += sizeof(pre_log_term);
  size += sizeof(pre_log_term);

  EncodeFixed32(p, commit_index);
  p += sizeof(commit_index);
  size += sizeof(commit_index);

  int32_t entries_size = entries.size();
  EncodeFixed32(p, entries_size);
  p += sizeof(entries_size);
  size += sizeof(entries_size);

  for (int32_t i = 0; i < entries_size; ++i) {
    int32_t bytes = entries[i].ToString(p, len - size);
    assert(bytes > 0);
    p += bytes;
    size += bytes;
  }

  assert(size <= len);
  return size;
}

inline int32_t AppendEntries::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

inline int32_t AppendEntries::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  uint64_t u64 = 0;
  int32_t size = 0;
  int32_t len2 = len;

  u64 = DecodeFixed64(p);
  src.FromU64(u64);
  p += sizeof(u64);
  size += sizeof(u64);
  len2 -= sizeof(u64);

  u64 = DecodeFixed64(p);
  dest.FromU64(u64);
  p += sizeof(u64);
  size += sizeof(u64);
  len2 -= sizeof(u64);

  term = DecodeFixed64(p);
  p += sizeof(term);
  size += sizeof(term);
  len2 -= sizeof(term);

  uid = DecodeFixed32(p);
  p += sizeof(uid);
  size += sizeof(uid);
  len2 -= sizeof(uid);

  pre_log_index = DecodeFixed32(p);
  p += sizeof(pre_log_index);
  size += sizeof(pre_log_index);
  len2 -= sizeof(pre_log_index);

  pre_log_term = DecodeFixed64(p);
  p += sizeof(pre_log_term);
  size += sizeof(pre_log_term);
  len2 -= sizeof(pre_log_term);

  commit_index = DecodeFixed32(p);
  p += sizeof(commit_index);
  size += sizeof(commit_index);
  len2 -= sizeof(commit_index);

  int32_t entries_size = DecodeFixed32(p);
  p += sizeof(entries_size);
  size += sizeof(entries_size);
  len2 -= sizeof(entries_size);

  LogEntry entry;
  for (int32_t i = 0; i < entries_size; ++i) {
    int32_t bytes = entry.FromString(p, len2);
    assert(bytes > 0);
    p += bytes;
    size += bytes;
    len2 -= bytes;
    entries.push_back(entry);
  }
  assert(len2 >= 0);

  return size;
}

inline nlohmann::json AppendEntries::ToJson() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dest"] = dest.ToString();
  j[0]["term"] = term;
  j[0]["uid"] = U32ToHexStr(uid);
  j[1]["pre"] = pre_log_index;
  j[1]["pre-term"] = pre_log_term;
  j[1]["commit"] = commit_index;
  j[1]["entry-count"] = entries.size();
  for (size_t i = 0; i < entries.size(); ++i) {
    j[2]["entries"][i] = entries[i].ToJson();
  }
  return j;
}

inline nlohmann::json AppendEntries::ToJsonTiny() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dst"] = dest.ToString();
  j[0]["tm"] = term;
  j[0]["uid"] = U32ToHexStr(uid);
  j[1]["pre"] = pre_log_index;
  j[1]["pre-tm"] = pre_log_term;
  j[1]["cmt"] = commit_index;
  j[1]["cnt"] = entries.size();
  for (size_t i = 0; i < entries.size(); ++i) {
    j[2]["etr"][i] = entries[i].ToJsonTiny();
  }
  return j;
}

inline std::string AppendEntries::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["ae"] = ToJsonTiny();
  } else {
    j["append-entries"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
