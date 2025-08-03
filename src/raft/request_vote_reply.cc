#include "request_vote_reply.h"

namespace vraft {

int32_t RequestVoteReply::MaxBytes() {
  int32_t size = 0;
  size += sizeof(uint64_t);
  size += sizeof(uint64_t);
  size += sizeof(term);
  size += sizeof(uid);
  size += sizeof(send_ts);
  size += sizeof(elapse);
  size += sizeof(uint8_t);  // bool granted;
  size += sizeof(uint8_t);  // bool log_ok;
  size += sizeof(uint8_t);  // bool pre_vote;
  size += sizeof(uint8_t);  // bool interval_ok;
  size += sizeof(req_term);
  return size;
}

int32_t RequestVoteReply::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

int32_t RequestVoteReply::ToString(const char *ptr, int32_t len) {
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

  {
    uint8_t u8 = granted;
    EncodeFixed8(p, u8);
    p += sizeof(u8);
    size += sizeof(u8);
  }

  {
    uint8_t u8 = log_ok;
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

  {
    uint8_t u8 = interval_ok;
    EncodeFixed8(p, u8);
    p += sizeof(u8);
    size += sizeof(u8);
  }

  EncodeFixed64(p, req_term);
  p += sizeof(req_term);
  size += sizeof(req_term);

  assert(size <= len);
  return size;
}

int32_t RequestVoteReply::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t RequestVoteReply::FromString(const char *ptr, int32_t len) {
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

  {
    uint8_t u8 = DecodeFixed8(p);
    p += sizeof(u8);
    size += sizeof(u8);
    granted = u8;
  }

  {
    uint8_t u8 = DecodeFixed8(p);
    p += sizeof(u8);
    size += sizeof(u8);
    log_ok = u8;
  }

  {
    uint8_t u8 = DecodeFixed8(p);
    p += sizeof(u8);
    size += sizeof(u8);
    pre_vote = u8;
  }

  {
    uint8_t u8 = DecodeFixed8(p);
    p += sizeof(u8);
    size += sizeof(u8);
    interval_ok = u8;
  }

  req_term = DecodeFixed64(p);
  p += sizeof(req_term);
  size += sizeof(req_term);

  return size;
}

nlohmann::json RequestVoteReply::ToJson() {
  nlohmann::json j;
  j[0]["src"] = src.ToString();
  j[0]["dest"] = dest.ToString();
  j[0]["term"] = term;
  j[0]["uid"] = U32ToHexStr(uid);
  j[1]["grant"] = granted;
  j[1]["log-ok"] = log_ok;
  j[1]["pre-vote"] = pre_vote;
  j[1]["interval-ok"] = interval_ok;
  j[1]["req-term"] = req_term;
  j[2]["send_ts"] = send_ts;
  j[2]["elapse"] = elapse;
  return j;
}

nlohmann::json RequestVoteReply::ToJsonTiny() {
  nlohmann::json j;
  j["src"] = src.ToString();
  j["dst"] = dest.ToString();
  j["tm"] = term;
  j["gr"] = granted;
  j["ok"] = log_ok;
  j["pvt"] = pre_vote;
  j["iok"] = interval_ok;
  j["rtm"] = req_term;
  j["uid"] = U32ToHexStr(uid);
  j["send"] = send_ts;
  j["elapse"] = elapse;
  return j;
}

std::string RequestVoteReply::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  std::string key;
  if (pre_vote) {
    key.append("pre-");
  }
  if (tiny) {
    key.append("rvr");
  } else {
    key.append("request-vote-reply");
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
