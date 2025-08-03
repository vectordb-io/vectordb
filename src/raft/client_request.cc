#include "client_request.h"

namespace vraft {

int32_t ClientRequest::MaxBytes() {
  int32_t sz = 0;
  sz += sizeof(uid);
  sz += sizeof(uint32_t);  // ClientCmd cmd;
  sz += 2 * sizeof(int32_t);
  sz += data.size();
  return sz;
}

int32_t ClientRequest::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

int32_t ClientRequest::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    EncodeFixed32(p, uid);
    size += sizeof(uid);
    p += sizeof(uid);
  }

  {
    uint32_t u32 = ClientCmdToU32(cmd);
    EncodeFixed32(p, u32);
    size += sizeof(u32);
    p += sizeof(u32);
  }

  {
    Slice sls(data.c_str(), data.size());
    char *p2 = EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  assert(size <= len);
  return size;
}

int32_t ClientRequest::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t ClientRequest::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    uid = DecodeFixed32(p);
    p += sizeof(uid);
    size += sizeof(uid);
  }

  {
    uint32_t u32 = DecodeFixed32(p);
    p += sizeof(u32);
    size += sizeof(u32);
    cmd = U32ToClientCmd(u32);
  }

  {
    Slice result;
    Slice input(p, len - size);
    int32_t sz = DecodeString2(&input, &result);
    if (sz > 0) {
      data.clear();
      data.append(result.data(), result.size());
      size += sz;
      p += sz;
    }
  }

  return size;
}

nlohmann::json ClientRequest::ToJson() {
  nlohmann::json j;
  j["uid"] = U32ToHexStr(uid);
  j["cmd"] = ClientCmdToStr(cmd);
  j["data"] = StrToHexStr(data.c_str(), data.size());
  return j;
}

nlohmann::json ClientRequest::ToJsonTiny() { return ToJson(); }

std::string ClientRequest::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  std::string key = "cr-";
  key.append(ClientCmdToStr(cmd));

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
