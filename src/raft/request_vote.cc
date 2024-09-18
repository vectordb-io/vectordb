#include "request_vote.h"

namespace vraft {

int32_t RequestVote::MaxBytes() {
  int32_t size = 0;
  size += sizeof(uint64_t);
  size += sizeof(uint64_t);
  size += sizeof(term);
  size += sizeof(uid);
  size += sizeof(send_ts);
  size += sizeof(elapse);
  size += sizeof(last_log_term);
  size += sizeof(last_log_index);
  size += sizeof(uint8_t);  // bool leader_transfer;
  size += sizeof(uint8_t);  // bool pre_vote;
  return size;
}

int32_t RequestVote::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

int32_t RequestVote::ToString(const char *ptr, int32_t len) {
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

  {
    uint8_t u8 = leader_transfer;
    EncodeFixed8(p, u8);
    p += sizeof(u8);
    size += sizeof(u8);
  }

  {
    uint8_t u8 = pre_vote;
    EncodeFixed8(p, u8);
    p += sizeof(u8);
    size += sizeof(u8);
  }

  assert(size <= len);
  return size;
}

int32_t RequestVote::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t RequestVote::FromString(const char *ptr, int32_t len) {
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
    uint8_t u8 = DecodeFixed8(p);
    p += sizeof(u8);
    size += sizeof(u8);
    leader_transfer = u8;
  }

  {
    uint8_t u8 = DecodeFixed8(p);
    p += sizeof(u8);
    size += sizeof(u8);
    pre_vote = u8;
  }

  return size;
}

nlohmann::json RequestVote::ToJson() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dest"] = dest.ToString();
  j[0]["term"] = term;
  j[0]["uid"] = U32ToHexStr(uid);
  j[1]["last"] = last_log_index;
  j[1]["last-term"] = last_log_term;
  j[1]["leader-transfer"] = leader_transfer;
  j[1]["pre-vote"] = pre_vote;
  j[2]["send_ts"] = send_ts;
  j[2]["elapse"] = elapse;
  return j;
}

nlohmann::json RequestVote::ToJsonTiny() {
  nlohmann::json j;
  j["src"] = src.ToString();
  j["dst"] = dest.ToString();
  j["tm"] = term;
  j["last"] = last_log_index;
  j["ltm"] = last_log_term;
  j["ldtrs"] = leader_transfer;
  j["pvt"] = pre_vote;
  j["uid"] = U32ToHexStr(uid);
  j["send"] = send_ts;
  j["elapse"] = elapse;
  return j;
}

std::string RequestVote::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  std::string key;
  if (pre_vote) {
    key.append("pre-");
  }
  if (tiny) {
    key.append("rv");
  } else {
    key.append("request-vote");
  }

  if (tiny) {
    j[key] = ToJsonTiny();
  } else {
    j[key] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft
