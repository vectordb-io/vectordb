#include "timeout_now.h"

namespace vraft {

int32_t TimeoutNow::MaxBytes() {
  int32_t size = 0;
  size += sizeof(uint64_t);
  size += sizeof(uint64_t);
  size += sizeof(term);
  size += sizeof(uid);
  size += sizeof(send_ts);
  size += sizeof(elapse);
  size += sizeof(last_log_term);
  size += sizeof(last_log_index);
  size += sizeof(uint8_t);  // bool force;
  return size;
}

int32_t TimeoutNow::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

int32_t TimeoutNow::ToString(const char *ptr, int32_t len) {
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

  EncodeFixed32(p, last_log_index);
  p += sizeof(last_log_index);
  size += sizeof(last_log_index);

  EncodeFixed64(p, last_log_term);
  p += sizeof(last_log_term);
  size += sizeof(last_log_term);

  EncodeFixed8(p, force);
  p += sizeof(force);
  size += sizeof(force);

  assert(size <= len);
  return size;
}

int32_t TimeoutNow::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t TimeoutNow::FromString(const char *ptr, int32_t len) {
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

  last_log_index = DecodeFixed32(p);
  p += sizeof(last_log_index);
  size += sizeof(last_log_index);

  last_log_term = DecodeFixed64(p);
  p += sizeof(last_log_term);
  size += sizeof(last_log_term);

  {
    uint8_t u8;
    u8 = DecodeFixed64(p);
    p += sizeof(u8);
    size += sizeof(u8);
    force = u8;
  }

  return size;
}

nlohmann::json TimeoutNow::ToJson() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dest"] = dest.ToString();
  j[0]["term"] = term;
  j[0]["uid"] = U32ToHexStr(uid);
  j[1]["last"] = last_log_index;
  j[1]["last-term"] = last_log_term;
  j[1]["force"] = force;
  j[2]["send_ts"] = send_ts;
  j[2]["elapse"] = elapse;
  return j;
}

nlohmann::json TimeoutNow::ToJsonTiny() {
  nlohmann::json j;
  j["src"] = src.ToString();
  j["dst"] = dest.ToString();
  j["tm"] = term;
  j["last"] = last_log_index;
  j["ltm"] = last_log_term;
  j["uid"] = U32ToHexStr(uid);
  j["send"] = send_ts;
  j["elapse"] = elapse;
  j["fce"] = force;
  return j;
}

std::string TimeoutNow::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["ton"] = ToJsonTiny();
  } else {
    j["timeout-now"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft
