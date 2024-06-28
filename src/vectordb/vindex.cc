#include "vindex.h"

#include <cassert>

#include "allocator.h"
#include "coding.h"
#include "common.h"
#include "util.h"
#include "vindex_annoy.h"

namespace vectordb {

nlohmann::json VecResults::ToJson() {
  nlohmann::json j;
  int32_t i = 0;
  for (auto &r : results) {
    j[i++] = r.ToJsonString(false, true);
  }
  return j;
}

nlohmann::json VecResults::ToJsonTiny() { return ToJson(); }

std::string VecResults::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["results"] = ToJsonTiny();
  } else {
    j["results"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

std::string VecResults::ToPrintString() {
  std::string s;
  for (auto &r : results) {
    s.append(r.ToPrintString()).append("\n");
  }
  return s;
}

nlohmann::json VecResult::ToJson() {
  nlohmann::json j;
  j["key"] = key;
  j["attach_value"] = attach_value;
  j["distance"] = distance;
  return j;
}

nlohmann::json VecResult::ToJsonTiny() {
  nlohmann::json j;
  j["key"] = key;
  j["av"] = attach_value;
  j["dis"] = distance;
  return j;
}

std::string VecResult::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vr"] = ToJsonTiny();
  } else {
    j["vec-result"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

std::string VecResult::ToPrintString() {
  char buf[1024];
  snprintf(buf, sizeof(buf), "{distance:%f, key:%s, attach_value:%s}", distance,
           key.c_str(), attach_value.c_str());
  return std::string(buf);
}

int32_t VIndexParam::MaxBytes() {
  int32_t sz = 0;
  sz += 2 * sizeof(uint32_t);
  sz += path.size();
  sz += sizeof(timestamp);
  sz += sizeof(dim);
  sz += sizeof(uint8_t);
  sz += sizeof(uint8_t);
  sz += sizeof(annoy_tree_num);
  return sz;
}

int32_t VIndexParam::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t VIndexParam::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    vraft::Slice sls(path.c_str(), path.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  vraft::EncodeFixed64(p, timestamp);
  p += sizeof(timestamp);
  size += sizeof(timestamp);

  vraft::EncodeFixed32(p, dim);
  p += sizeof(dim);
  size += sizeof(dim);

  uint8_t u8 = index_type;
  vraft::EncodeFixed8(p, u8);
  p += sizeof(u8);
  size += sizeof(u8);

  u8 = distance_type;
  vraft::EncodeFixed8(p, u8);
  p += sizeof(u8);
  size += sizeof(u8);

  vraft::EncodeFixed32(p, annoy_tree_num);
  p += sizeof(annoy_tree_num);
  size += sizeof(annoy_tree_num);

  assert(size <= len);
  return size;
}

int32_t VIndexParam::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t VIndexParam::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    path.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = DecodeString2(&input, &result);
    if (sz > 0) {
      path.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  timestamp = vraft::DecodeFixed64(p);
  p += sizeof(timestamp);
  size += sizeof(timestamp);

  dim = vraft::DecodeFixed32(p);
  p += sizeof(dim);
  size += sizeof(dim);

  uint8_t u8 = vraft::DecodeFixed8(p);
  p += sizeof(u8);
  size += sizeof(u8);
  index_type = U82VIndexType(u8);

  u8 = vraft::DecodeFixed8(p);
  p += sizeof(u8);
  size += sizeof(u8);
  distance_type = U82DistanceType(u8);

  annoy_tree_num = vraft::DecodeFixed32(p);
  p += sizeof(annoy_tree_num);
  size += sizeof(annoy_tree_num);

  return size;
}

nlohmann::json VIndexParam::ToJson() {
  nlohmann::json j;
  j["path"] = path;
  j["timestamp"][0] = timestamp;
  j["timestamp"][1] = vraft::NsToString(timestamp);
  j["dim"] = dim;
  j["index_type"][0] = index_type;
  j["index_type"][1] = VIndexType2Str(index_type);
  j["distance_type"][0] = distance_type;
  j["distance_type"][1] = DistanceType2Str(distance_type);
  j["annoy_tree_num"] = annoy_tree_num;
  return j;
}

nlohmann::json VIndexParam::ToJsonTiny() { return ToJson(); }

std::string VIndexParam::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vindex-param"] = ToJsonTiny();
  } else {
    j["vindex-param"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

VindexSPtr Create(VIndexType index_type, const std::string &path, VEngineSPtr v,
                  VIndexParam &param) {
  VindexSPtr index_sp;
  switch (index_type) {
    case kIndexAnnoy: {
      index_sp = std::make_shared<VindexAnnoy>(param, v);
      assert(index_sp);
      break;
    }

    default:
      assert(0);
  }

  return index_sp;
}

}  // namespace vectordb
