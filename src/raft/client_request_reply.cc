#include "client_request_reply.h"

namespace vraft {

int32_t ClientRequestReply::MaxBytes() {
  int32_t sz = 0;
  sz += sizeof(uid);
  sz += sizeof(code);
  sz += 2 * sizeof(int32_t);
  sz += msg.size();
  sz += 2 * sizeof(int32_t);
  sz += data.size();
  return sz;
}

int32_t ClientRequestReply::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr = reinterpret_cast<char *>(DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  DefaultAllocator().Free(ptr);
  return size;
}

int32_t ClientRequestReply::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    EncodeFixed32(p, uid);
    size += sizeof(uid);
    p += sizeof(uid);
  }

  {
    EncodeFixed32(p, code);
    size += sizeof(code);
    p += sizeof(code);
  }

  {
    Slice sls(msg.c_str(), msg.size());
    char *p2 = EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
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

int32_t ClientRequestReply::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t ClientRequestReply::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    uid = DecodeFixed32(p);
    p += sizeof(uid);
    size += sizeof(uid);
  }

  {
    code = DecodeFixed32(p);
    p += sizeof(code);
    size += sizeof(code);
  }

  {
    Slice result;
    Slice input(p, len - size);
    int32_t sz = DecodeString2(&input, &result);
    if (sz > 0) {
      msg.clear();
      msg.append(result.data(), result.size());
      size += sz;
      p += sz;
    }
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

nlohmann::json ClientRequestReply::ToJson() {
  nlohmann::json j;
  j["uid"] = U32ToHexStr(uid);
  j["code"] = code;
  j["msg"] = msg;
  j["data"] = data;
  return j;
}

nlohmann::json ClientRequestReply::ToJsonTiny() { return ToJson(); }

std::string ClientRequestReply::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["crr"] = ToJsonTiny();
  } else {
    j["client-request-reply"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vraft
