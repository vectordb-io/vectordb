#include "vstore_msg.h"

namespace vstore {

int32_t VstoreGet::MaxBytes() {
  int32_t sz = 0;
  sz += sizeof(uid);
  sz += 2 * sizeof(int32_t);
  sz += key.size();
  return sz;
}

int32_t VstoreGet::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t VstoreGet::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  vraft::EncodeFixed32(p, uid);
  p += sizeof(uid);
  size += sizeof(uid);

  vraft::Slice sls(key.c_str(), key.size());
  char *p2 = vraft::EncodeString2(p, len - size, sls);
  size += (p2 - p);
  p = p2;

  assert(size <= len);
  return size;
}

int32_t VstoreGet::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t VstoreGet::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  uid = vraft::DecodeFixed32(p);
  p += sizeof(uid);
  size += sizeof(uid);

  vraft::Slice result;
  vraft::Slice input(p, len - size);
  int32_t sz = vraft::DecodeString2(&input, &result);
  if (sz > 0) {
    key.clear();
    key.append(result.data(), result.size());
    size += sz;
  }

  return size;
}

nlohmann::json VstoreGet::ToJson() {
  nlohmann::json j;
  j["uid"] = vraft::U32ToHexStr(uid);
  j["key"] = vraft::StrToHexStr(key.c_str(), key.size());
  return j;
}

nlohmann::json VstoreGet::ToJsonTiny() {
  nlohmann::json j;
  j["uid"] = vraft::U32ToHexStr(uid);
  j["key"] = vraft::StrToHexStr(key.c_str(), key.size());
  return j;
}

std::string VstoreGet::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vs-get"] = ToJsonTiny();
  } else {
    j["vstore-get"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

//-------------------------------------------------------------------------------

int32_t VstoreGetReply::MaxBytes() {
  int32_t sz = 0;
  sz += sizeof(uid);
  sz += 2 * sizeof(int32_t);
  sz += value.size();
  return sz;
}

int32_t VstoreGetReply::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t VstoreGetReply::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  vraft::EncodeFixed32(p, uid);
  p += sizeof(uid);
  size += sizeof(uid);

  vraft::Slice sls(value.c_str(), value.size());
  char *p2 = vraft::EncodeString2(p, len - size, sls);
  size += (p2 - p);
  p = p2;

  assert(size <= len);
  return size;
}

int32_t VstoreGetReply::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t VstoreGetReply::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  uid = vraft::DecodeFixed32(p);
  p += sizeof(uid);
  size += sizeof(uid);

  vraft::Slice result;
  vraft::Slice input(p, len - size);
  int32_t sz = vraft::DecodeString2(&input, &result);
  if (sz > 0) {
    value.clear();
    value.append(result.data(), result.size());
    size += sz;
  }

  return size;
}

nlohmann::json VstoreGetReply::ToJson() {
  nlohmann::json j;
  j["uid"] = vraft::U32ToHexStr(uid);
  j["val"] = vraft::StrToHexStr(value.c_str(), value.size());
  return j;
}

nlohmann::json VstoreGetReply::ToJsonTiny() {
  nlohmann::json j;
  j["uid"] = vraft::U32ToHexStr(uid);
  j["value"] = vraft::StrToHexStr(value.c_str(), value.size());
  return j;
}

std::string VstoreGetReply::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vs-get-r"] = ToJsonTiny();
  } else {
    j["vstore-get-reply"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

}  // namespace vstore
