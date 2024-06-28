#ifndef VRAFT_RAFT_LOG_H_
#define VRAFT_RAFT_LOG_H_

#include <stdint.h>

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#include "allocator.h"
#include "coding.h"
#include "common.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "nlohmann/json.hpp"
#include "slice.h"
#include "util.h"

namespace vraft {

enum EntryType {
  kUnknown = 0,
  kData,
  kNoop,
  kConfig,
};

inline EntryType U32ToEntryType(uint32_t u32) {
  switch (u32) {
    case 0:
      return kUnknown;
    case 1:
      return kData;
    case 2:
      return kNoop;
    case 3:
      return kConfig;
    default:
      assert(0);
  }
}

inline const char *EntryTypeToStr(enum EntryType e) {
  switch (e) {
    case kUnknown:
      return "Unknown";
    case kData:
      return "Data";
    case kNoop:
      return "Noop";
    case kConfig:
      return "Config";
    default:
      assert(0);
  }
}

// log_index: 1 2 3
// term_key : 1 3 5
// value_key: 2 4 6
RaftIndex LogIndexToMetaIndex(RaftIndex log_index);
RaftIndex MetaIndexToLogIndex(RaftIndex term_index);
RaftIndex LogIndexToDataIndex(RaftIndex log_index);
RaftIndex DataIndexToLogIndex(RaftIndex value_index);

// log-index --> term-index --> [term, type, pre_check_all, check_this,
// check_all] log-index --> value-index --> [value]
struct MetaValue {
  RaftTerm term;
  EntryType type;  // uint32_t
  uint32_t pre_chk_all;
  uint32_t chk_ths;
  uint32_t chk_all;
};
using MetaValuePtr = std::shared_ptr<MetaValue>;

int32_t MetaValueBytes();
void EncodeDataKey(char *buf, int32_t len, RaftIndex log_index);
void EncodeMetaKey(char *buf, int32_t len, RaftIndex log_index);
void EncodeMetaValue(char *buf, int32_t len, MetaValue &meta);
void DecodeMetaValue(const char *buf, int32_t len, MetaValue &meta);

const leveldb::Comparator *U32Comparator();

struct ZeroValue {};

const char kZeroKey[sizeof(RaftIndex)] = {0};
void EncodeZeroValue(char *buf, RaftIndex append, uint32_t checksum);
void DecodeZeroValue(const char *buf, RaftIndex &append, uint32_t &checksum);

struct AppendEntry {
  RaftTerm term;   // uint64_t
  EntryType type;  // uint32_t
  std::string value;
};

struct LogEntry;
using LogEntryPtr = std::shared_ptr<LogEntry>;

struct LogEntry {
  RaftIndex index;
  uint32_t pre_chk_all;  // current checksum of log
  uint32_t chk_ths;      // checksum [index + term + type + value]
  uint32_t chk_all;      // checksum [pre_chk_all + chk_ths]
  AppendEntry append_entry;

  void CheckSum();
  void CheckThis();
  void CheckAll();

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

inline void LogEntry::CheckSum() {
  CheckThis();
  CheckAll();
}

// checksum [index + term + type + value]
inline void LogEntry::CheckThis() {
  int32_t max_bytes = sizeof(RaftIndex) + sizeof(append_entry.term) +
                      sizeof(uint32_t) + 2 * sizeof(int32_t) +
                      append_entry.value.size();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  char *p = ptr;
  int32_t size = 0;

  EncodeFixed32(p, index);
  p += sizeof(index);
  size += sizeof(index);

  EncodeFixed64(p, append_entry.term);
  p += sizeof(append_entry.term);
  size += sizeof(append_entry.term);

  EncodeFixed32(p, append_entry.type);
  p += sizeof(append_entry.type);
  size += sizeof(append_entry.type);

  EncodeFixed32(p, index);
  p += sizeof(index);
  size += sizeof(index);

  Slice sls(append_entry.value.c_str(), append_entry.value.size());
  char *p2 = EncodeString2(p, max_bytes - size, sls);
  size += (p2 - p);

  chk_ths = Crc32(ptr, size);
  DefaultAllocator().Free(ptr);
}

inline void LogEntry::CheckAll() {
  char buf[sizeof(uint32_t) * 2];
  char *p = buf;
  int32_t size = 0;

  EncodeFixed32(p, pre_chk_all);
  p += sizeof(pre_chk_all);
  size += sizeof(pre_chk_all);

  EncodeFixed32(p, chk_ths);
  p += sizeof(chk_ths);
  size += sizeof(chk_ths);

  chk_all = Crc32(buf, sizeof(uint32_t) * 2);
}

inline int32_t LogEntry::MaxBytes() {
  return sizeof(RaftIndex) + sizeof(pre_chk_all) + sizeof(chk_ths) +
         sizeof(chk_all) + sizeof(append_entry.term) +
         sizeof(append_entry.type) + 2 * sizeof(int32_t) +
         append_entry.value.size();
}

inline int32_t LogEntry::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

inline int32_t LogEntry::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  EncodeFixed32(p, index);
  p += sizeof(index);
  size += sizeof(index);

  EncodeFixed32(p, chk_ths);
  p += sizeof(chk_ths);
  size += sizeof(chk_ths);

  EncodeFixed32(p, chk_all);
  p += sizeof(chk_all);
  size += sizeof(chk_all);

  EncodeFixed64(p, append_entry.term);
  p += sizeof(append_entry.term);
  size += sizeof(append_entry.term);

  uint32_t u32 = append_entry.type;
  EncodeFixed32(p, u32);
  p += sizeof(u32);
  size += sizeof(u32);

  Slice sls(append_entry.value.c_str(), append_entry.value.size());
  char *p2 = EncodeString2(p, len - size, sls);
  size += (p2 - p);

  assert(size <= len);
  return size;
}

inline int32_t LogEntry::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

inline int32_t LogEntry::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  index = DecodeFixed32(p);
  p += sizeof(index);
  size += sizeof(index);

  chk_ths = DecodeFixed32(p);
  p += sizeof(chk_ths);
  size += sizeof(chk_ths);

  chk_all = DecodeFixed32(p);
  p += sizeof(chk_all);
  size += sizeof(chk_all);

  append_entry.term = DecodeFixed64(p);
  p += sizeof(append_entry.term);
  size += sizeof(append_entry.term);

  uint32_t u32 = DecodeFixed32(p);
  p += sizeof(u32);
  size += sizeof(u32);
  append_entry.type = U32ToEntryType(u32);

  Slice result;
  Slice input(p, len - sizeof(index) - sizeof(append_entry.term));
  int32_t sz = DecodeString2(&input, &result);
  if (sz > 0) {
    append_entry.value.clear();
    append_entry.value.append(result.data(), result.size());
    size += sz;

  } else {
    return -1;
  }

  return size;
}

inline nlohmann::json LogEntry::ToJson() {
  nlohmann::json j;
  j["index"] = index;
  j["term"] = append_entry.term;
  j["type"] = std::string(EntryTypeToStr(append_entry.type));
  j["value"] =
      StrToHexStr(append_entry.value.c_str(), append_entry.value.size());
  j["pre_chk_all"] = U32ToHexStr(pre_chk_all);
  j["chk_ths"] = U32ToHexStr(chk_ths);
  j["chk_all"] = U32ToHexStr(chk_all);
  j["this"] = PointerToHexStr(this);
  return j;
}

inline nlohmann::json LogEntry::ToJsonTiny() {
  nlohmann::json j;
  j["idx"] = index;
  j["tm"] = append_entry.term;
  j["tp"] = std::string(EntryTypeToStr(append_entry.type));
  j["val"] = StrToHexStr(append_entry.value.c_str(), append_entry.value.size());
  j["ck"][0] = U32ToHexStr(pre_chk_all);
  j["ck"][1] = U32ToHexStr(chk_ths);
  j["ck"][2] = U32ToHexStr(chk_all);
  j["ts"] = PointerToHexStr(this);
  return j;
}

inline std::string LogEntry::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["entry"] = ToJsonTiny();
  } else {
    j["log_entry"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

// Wraps an instance whose destructor is never called.
//
// This is intended for use with function-level static variables.
template <typename InstanceType>
class NoDestructor {
 public:
  template <typename... ConstructorArgTypes>
  explicit NoDestructor(ConstructorArgTypes &&... constructor_args) {
    static_assert(sizeof(instance_storage_) >= sizeof(InstanceType),
                  "instance_storage_ is not large enough to hold the instance");
    static_assert(
        alignof(decltype(instance_storage_)) >= alignof(InstanceType),
        "instance_storage_ does not meet the instance's alignment requirement");
    new (&instance_storage_)
        InstanceType(std::forward<ConstructorArgTypes>(constructor_args)...);
  }

  ~NoDestructor() = default;

  NoDestructor(const NoDestructor &) = delete;
  NoDestructor &operator=(const NoDestructor &) = delete;

  InstanceType *get() {
    return reinterpret_cast<InstanceType *>(&instance_storage_);
  }

 private:
  typename std::aligned_storage<sizeof(InstanceType),
                                alignof(InstanceType)>::type instance_storage_;
};

class U32ComparatorImpl : public leveldb::Comparator {
 public:
  U32ComparatorImpl() {}

  U32ComparatorImpl(const U32ComparatorImpl &) = delete;
  U32ComparatorImpl &operator=(const U32ComparatorImpl &) = delete;

  virtual const char *Name() const { return "leveldb.U32ComparatorImpl"; }

  virtual int Compare(const leveldb::Slice &a, const leveldb::Slice &b) const;
  virtual void FindShortestSeparator(std::string *start,
                                     const leveldb::Slice &limit) const;
  virtual void FindShortSuccessor(std::string *key) const;

  ~U32ComparatorImpl();

 private:
};

class Tracer;
class RaftLog;
using RaftLogUPtr = std::unique_ptr<RaftLog>;

class RaftLog final {
 public:
  RaftLog(const std::string &path);
  ~RaftLog();
  RaftLog(const RaftLog &t) = delete;
  RaftLog &operator=(const RaftLog &t) = delete;
  void Init();
  void Check();
  bool IndexValid(RaftIndex index);

  int32_t AppendOne(AppendEntry &entry, Tracer *tracer);
  int32_t AppendSome(std::vector<AppendEntry> &entries);
  int32_t DeleteFrom(RaftIndex from_index);
  int32_t DeleteUtil(RaftIndex to_index);
  int32_t Get(RaftIndex index, LogEntry &entry);
  int32_t GetMeta(RaftIndex index, MetaValue &meta);
  int32_t GetValue(RaftIndex index, std::string *value);
  LogEntryPtr LastEntry();
  MetaValuePtr LastMeta();

  void EnableCheckSum() { checksum_ = true; }
  void DisableCheckSum() { checksum_ = false; }

  RaftIndex First() const { return first_; };
  RaftIndex Last() const { return last_; }
  RaftIndex Append() const { return append_; }
  RaftIndex LastCheck() const { return last_checksum_; }

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

 private:
  RaftIndex first_;
  RaftIndex last_;
  RaftIndex append_;
  bool checksum_;
  uint32_t last_checksum_;

  std::string path_;
  leveldb::Options db_options_;
  std::shared_ptr<leveldb::DB> db_;
};

inline RaftLog::~RaftLog() {}

inline nlohmann::json RaftLog::ToJson() {
  nlohmann::json j;
  j["first"] = first_;
  j["last"] = last_;
  j["append"] = append_;
  j["checksum"] = U32ToHexStr(last_checksum_);
  MetaValuePtr ptr = LastMeta();
  if (ptr) {
    j["last_term"] = ptr->term;
  } else {
    j["last_term"] = 0;
  }

  return j;
}

inline nlohmann::json RaftLog::ToJsonTiny() {
  nlohmann::json j;
  j[0] = first_;
  j[1] = last_;
  j[2] = append_;
  j[3]["chk"] = U32ToHexStr(last_checksum_);
  MetaValuePtr ptr = LastMeta();
  if (ptr) {
    j[3]["ltm"] = ptr->term;
  } else {
    j[3]["ltm"] = 0;
  }
  return j;
}

inline std::string RaftLog::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["rlog"] = ToJsonTiny();
  } else {
    j["raft_log"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
