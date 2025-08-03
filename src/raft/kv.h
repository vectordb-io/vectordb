#ifndef VRAFT_KV_H_
#define VRAFT_KV_H_

#include <string>

#include "allocator.h"
#include "common.h"
#include "message.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "util.h"

namespace vraft {

struct KV {
  std::string key;
  std::string value;

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

inline int32_t KV::MaxBytes() {
  int32_t size = 0;
  size += 2 * sizeof(int32_t) + key.size();
  size += 2 * sizeof(int32_t) + value.size();
  return size;
}

inline int32_t KV::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

inline int32_t KV::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    Slice sls(key.c_str(), key.size());
    char *p2 = EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  {
    Slice sls(value.c_str(), value.size());
    char *p2 = EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  assert(size <= len);
  return size;
}

inline int32_t KV::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

inline int32_t KV::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    Slice result;
    Slice input(p, len - size);
    int32_t sz = DecodeString2(&input, &result);
    if (sz > 0) {
      key.clear();
      key.append(result.data(), result.size());
      size += sz;
      p += sz;
    }
  }

  {
    Slice result;
    Slice input(p, len - size);
    int32_t sz = DecodeString2(&input, &result);
    if (sz > 0) {
      value.clear();
      value.append(result.data(), result.size());
      size += sz;
      p += sz;
    }
  }

  return size;
}

inline nlohmann::json KV::ToJson() {
  nlohmann::json j;
  j["key"] = key;
  j["value"] = value;
  return j;
}

inline nlohmann::json KV::ToJsonTiny() {
  nlohmann::json j;
  j["k"] = key;
  j["v"] = value;
  return j;
}

inline std::string KV::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["kv"] = ToJsonTiny();
  } else {
    j["kv"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft

#endif
