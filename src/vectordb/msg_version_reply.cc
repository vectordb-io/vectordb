#include "msg_version_reply.h"

namespace vectordb {

int32_t MsgVersionReply::MaxBytes() {
  return sizeof(seqid) + 2 * sizeof(int32_t) + version.size();
}

int32_t MsgVersionReply::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t MsgVersionReply::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  vraft::EncodeFixed64(p, seqid);
  p += sizeof(seqid);
  size += sizeof(seqid);

  vraft::Slice sls(version.c_str(), version.size());
  char *p2 = vraft::EncodeString2(p, len - size, sls);
  size += (p2 - p);

  assert(size <= len);
  return size;
}

int32_t MsgVersionReply::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t MsgVersionReply::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  seqid = vraft::DecodeFixed64(p);
  p += sizeof(seqid);
  size += sizeof(seqid);

  vraft::Slice result;
  vraft::Slice input(p, len - size);
  int32_t sz = DecodeString2(&input, &result);
  if (sz > 0) {
    version.clear();
    version.append(result.data(), result.size());
    size += sz;
  }
  return size;
}

nlohmann::json MsgVersionReply::ToJson() {
  nlohmann::json j;
  j["seqid"] = seqid;
  j["version"] = version;
  return j;
}

nlohmann::json MsgVersionReply::ToJsonTiny() {
  nlohmann::json j;
  j["seq"] = seqid;
  j["ver"] = version;
  return j;
}

std::string MsgVersionReply::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["ver-r"] = ToJsonTiny();
  } else {
    j["version-reply"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vectordb
