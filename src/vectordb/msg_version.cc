#include "msg_version.h"

namespace vectordb {

int32_t MsgVersion::MaxBytes() { return sizeof(seqid); }

int32_t MsgVersion::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t MsgVersion::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  vraft::EncodeFixed64(p, seqid);
  p += sizeof(seqid);
  size += sizeof(seqid);

  assert(size <= len);
  return size;
}

int32_t MsgVersion::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t MsgVersion::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  seqid = vraft::DecodeFixed64(p);
  p += sizeof(seqid);
  size += sizeof(seqid);

  return size;
}

nlohmann::json MsgVersion::ToJson() {
  nlohmann::json j;
  j["seqid"] = seqid;
  return j;
}

nlohmann::json MsgVersion::ToJsonTiny() {
  nlohmann::json j;
  j["seq"] = seqid;
  return j;
}

std::string MsgVersion::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["ver"] = ToJsonTiny();
  } else {
    j["version"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vectordb
