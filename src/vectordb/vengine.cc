#include "vengine.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "allocator.h"
#include "coding.h"
#include "common.h"
#include "util.h"
#include "vindex_annoy.h"
#include "vindex_manager.h"
#include "vraft_logger.h"

namespace vectordb {

int32_t Vec::MaxBytes() {
  int32_t sz = 0;
  sz += sizeof(uint32_t);
  for (size_t i = 0; i < data.size(); ++i) {
    sz += sizeof(float);
  }
  return sz;
}

int32_t Vec::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t Vec::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  uint32_t u32 = data.size();
  vraft::EncodeFixed32(p, u32);
  p += sizeof(u32);
  size += sizeof(u32);

  for (size_t i = 0; i < data.size(); ++i) {
    float f32 = data[i];
    vraft::EncodeFloat(p, f32);
    p += sizeof(f32);
    size += sizeof(f32);
  }

  assert(size <= len);
  return size;
}

int32_t Vec::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t Vec::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  uint32_t u32 = 0;
  u32 = vraft::DecodeFixed32(p);
  p += sizeof(u32);
  size += sizeof(u32);

  for (uint32_t i = 0; i < u32; ++i) {
    float f32 = vraft::DecodeFloat(p);
    p += sizeof(f32);
    size += sizeof(f32);
    data.push_back(f32);
  }

  return size;
}

nlohmann::json Vec::ToJson() {
  nlohmann::json j;
  for (size_t i = 0; i < data.size(); ++i) {
    float f32 = data[i];
    j[i] = f32;
  }
  return j;
}

nlohmann::json Vec::ToJsonTiny() {
  nlohmann::json j;
  for (size_t i = 0; i < data.size(); ++i) {
    float f32 = data[i];
    j[i] = f32;
  }
  return j;
}

std::string Vec::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vec"] = ToJsonTiny();
  } else {
    j["vec"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

int32_t VecValue::MaxBytes() {
  int32_t sz = 0;
  sz += vec.MaxBytes();
  sz += 2 * sizeof(uint32_t);
  sz += attach_value.size();
  return sz;
}

int32_t VecValue::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t VecValue::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  int32_t bytes = vec.ToString(p, len - size);
  assert(bytes > 0);
  p += bytes;
  size += bytes;

  {
    vraft::Slice sls(attach_value.c_str(), attach_value.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  assert(size <= len);
  return size;
}

int32_t VecValue::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t VecValue::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  int32_t bytes = vec.FromString(p, len - size);
  assert(bytes >= 0);
  p += bytes;
  size += bytes;

  {
    attach_value.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = DecodeString2(&input, &result);
    if (sz > 0) {
      attach_value.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  return size;
}

nlohmann::json VecValue::ToJson() {
  nlohmann::json j;
  j["vec"] = vec.ToJson();
  j["attach_value"] = attach_value;
  return j;
}

nlohmann::json VecValue::ToJsonTiny() {
  nlohmann::json j;
  j["vec"] = vec.ToJsonTiny();
  j["av"] = attach_value;
  return j;
}

std::string VecValue::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vv"] = ToJsonTiny();
  } else {
    j["vec-value"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

int32_t VecObj::MaxBytes() {
  int32_t sz = 0;
  sz += 2 * sizeof(uint32_t);
  sz += key.size();
  sz += vv.MaxBytes();
  return sz;
}

int32_t VecObj::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t VecObj::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    vraft::Slice sls(key.c_str(), key.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  int32_t bytes = vv.ToString(p, len - size);
  assert(bytes > 0);
  p += bytes;
  size += bytes;

  assert(size <= len);
  return size;
}

int32_t VecObj::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t VecObj::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    key.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = DecodeString2(&input, &result);
    if (sz > 0) {
      key.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  int32_t bytes = vv.FromString(p, len - size);
  assert(bytes >= 0);
  p += bytes;
  size += bytes;

  return size;
}

nlohmann::json VecObj::ToJson() {
  nlohmann::json j;
  j["key"] = key;
  j["value"] = vv.ToJson();
  return j;
}

nlohmann::json VecObj::ToJsonTiny() {
  nlohmann::json j;
  j["key"] = key;
  j["val"] = vv.ToJsonTiny();
  return j;
}

std::string VecObj::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vo"] = ToJsonTiny();
  } else {
    j["vec-obj"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

VEngine::VEngine(const std::string &path, int32_t dim)
    : path_(path),
      meta_path_(path + "/meta"),
      data_path_(path + "/data"),
      index_path_(path + "/index") {
  if (!vraft::IsDirExist(path_)) {
    MkDir();

    // init meta
    auto meta = std::make_shared<EngineMeta>(meta_path_);
    assert(meta);
    meta->SetDim(dim);
  }

  Init();
}

std::shared_ptr<leveldb::DB> VEngine::db() { return db_; }

int32_t VEngine::Dim() const { return meta_->dim(); }

int32_t VEngine::Put(const std::string &key, VecValue &vv) {
  if (vv.vec.dim() != meta_->dim()) {
    vraft::vraft_logger.FError("dim error, %d != %d", vv.vec.dim(),
                               meta_->dim());
    return -1;
  }

  std::string value;
  vv.ToString(value);

  leveldb::Status s;
  leveldb::WriteOptions wo;
  wo.sync = true;
  s = db_->Put(wo, key, value);
  assert(s.ok());
  return 0;
}

int32_t VEngine::Get(const std::string &key, VecObj &vo) const {
  VecValue vv;
  leveldb::Status s;
  std::string value;
  s = db_->Get(leveldb::ReadOptions(), key, &value);
  if (s.ok()) {
    int32_t bytes = vv.FromString(value);
    assert(bytes > 0);
    vo.key = key;
    vo.vv = vv;
    return 0;
  } else if (s.IsNotFound()) {
    std::string hex_key = vraft::StrToHexStr(key.c_str(), key.size());
    vraft::vraft_logger.FInfo("key:%s value not found", hex_key.c_str());
    return -2;  // use Status instead!!
  }
  return -1;
}

int32_t VEngine::Delete(const std::string &key) {
  leveldb::Slice sls(key);
  leveldb::WriteOptions wo;
  wo.sync = true;
  auto s = db_->Delete(wo, sls);
  assert(s.ok());
  return 0;
}

/*
key_0; 0.297595, 0.585204, 0.253584, 0.193170, 0.118066; attach_value_0
key_1; 0.133834, 0.533521, 0.041994, 0.736852, 0.453392; attach_value_1
key_2; 0.004477, 0.069064, 0.204226, 0.180199, 0.125557; attach_value_2
*/
int32_t VEngine::Load(const std::string &file_path) {
  std::ifstream file(file_path);
  if (!file.is_open()) {
    vraft::vraft_logger.FError("failed to open file: %s", file_path.c_str());
    return -1;
  }

  std::string line;
  int32_t line_num = 1;
  while (std::getline(file, line)) {
    std::cout << "load line " << line_num++ << ": " << line << std::endl;

    vraft::DelSpace(line);
    std::vector<std::string> result;
    vraft::Split(line, ';', result);

    if (result.size() != 3) {
      return -1;
    }

    std::string key = result[0];
    VecValue vv;
    std::string vec_str = result[1];
    std::vector<std::string> r;
    vraft::Split(vec_str, ',', r);
    for (auto f_str : r) {
      float f32;
      sscanf(f_str.c_str(), "%f", &f32);
      vv.vec.data.push_back(f32);
    }
    vv.attach_value = result[2];

    int32_t rv = Put(key, vv);
    if (rv != 0) {
      return rv;
    }
  }

  file.close();
  return 0;
}

bool VEngine::HasIndex() const { return index_manager_->HasIndex(); }

int32_t VEngine::AddIndex(AddIndexParam add_param) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s/%s_%lu", index_path_.c_str(),
           VIndexType2Str(add_param.index_type), add_param.timestamp);

  VIndexParam param;
  param.path = buf;
  param.timestamp = add_param.timestamp;
  param.dim = add_param.dim;
  param.index_type = add_param.index_type;
  param.distance_type = add_param.distance_type;
  param.annoy_tree_num = add_param.annoy_tree_num;
  VindexAnnoy *index = new VindexAnnoy(param, shared_from_this());
  assert(index);
  VindexSPtr vindex;
  vindex.reset(index);
  index_manager_->Add(vindex);

  return 0;
}

int32_t VEngine::GetKNN(const std::string &key, std::vector<VecResult> &results,
                        int limit) {
  results.clear();
  auto sptr = index_manager_->GetNewest();
  if (sptr) {
    return sptr->GetKNN(key, results, limit);
  }
  return -1;
}

int32_t VEngine::GetKNN(const std::vector<float> &vec,
                        std::vector<VecResult> &results, int limit) {
  results.clear();
  auto sptr = index_manager_->GetNewest();
  if (sptr) {
    return sptr->GetKNN(vec, results, limit);
  }
  return -1;
}

nlohmann::json VEngine::ToJson() {
  nlohmann::json j;
  j["meta"] = meta_->ToJson();
  j["indices"] = index_manager_->ToJson();
  return j;
}

nlohmann::json VEngine::ToJsonTiny() {
  nlohmann::json j;
  j[0] = meta_->ToJson();
  return j;
}

std::string VEngine::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["ve"] = ToJsonTiny();
  } else {
    j["vengine"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

void VEngine::Init() {
  // init data
  db_options_.create_if_missing = true;
  db_options_.error_if_exists = false;
  leveldb::DB *dbptr;
  leveldb::Status status = leveldb::DB::Open(db_options_, data_path_, &dbptr);
  if (!status.ok()) {
    vraft::vraft_logger.FError("leveldb open %s error, %s", data_path_.c_str(),
                               status.ToString().c_str());
    assert(0);
  }
  db_.reset(dbptr);

  // init meta
  meta_ = std::make_shared<EngineMeta>(meta_path_);
  assert(meta_);

  // init index manager
  index_manager_ = std::make_shared<VindexManager>();
  assert(index_manager_);
}

void VEngine::LoadIndex() {
  std::vector<std::string> index_paths;
  vraft::ListDir(index_path_, index_paths);
  for (auto &index_path : index_paths) {
    VIndexParam param;
    param.path = index_path;
    param.timestamp = 0;
    param.dim = meta_->dim();
    param.index_type = kIndexAnnoy;
    param.distance_type = kCosine;
    param.annoy_tree_num = 0;

    VindexAnnoy *index = new VindexAnnoy(param, shared_from_this());
    assert(index);
    VindexSPtr vindex;
    vindex.reset(index);
    index_manager_->Add(vindex);
  }
}

void VEngine::MkDir() {
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "mkdir -p %s", path_.c_str());
  system(cmd);
  // snprintf(cmd, sizeof(cmd), "mkdir -p %s", meta_path_.c_str());
  // system(cmd);
  // snprintf(cmd, sizeof(cmd), "mkdir -p %s", data_path_.c_str());
  // system(cmd);
  snprintf(cmd, sizeof(cmd), "mkdir -p %s", index_path_.c_str());
  system(cmd);
}

}  // namespace vectordb
