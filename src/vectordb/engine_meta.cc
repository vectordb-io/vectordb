#include "engine_meta.h"

#include "coding.h"
#include "common.h"
#include "vraft_logger.h"

namespace vectordb {

EngineMeta::EngineMeta(const std::string &path) : path_(path), dim_(0) {
  Init();
}

void EngineMeta::SetDim(int32_t dim) {
  dim_ = dim;
  PersistDim();
}

nlohmann::json EngineMeta::ToJson() {
  nlohmann::json j;
  j["dim"] = dim_;
  return j;
}

nlohmann::json EngineMeta::ToJsonTiny() {
  nlohmann::json j;
  j["dim"] = dim_;
  return j;
}

std::string EngineMeta::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["meta"] = ToJsonTiny();
  } else {
    j["meta"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

void EngineMeta::Init() {
  db_options_.create_if_missing = true;
  db_options_.error_if_exists = false;
  leveldb::DB *dbptr;
  leveldb::Status status = leveldb::DB::Open(db_options_, path_, &dbptr);
  if (!status.ok()) {
    vraft::vraft_logger.FError("leveldb open %s error, %s", path_.c_str(),
                               status.ToString().c_str());
    assert(0);
  }
  db_.reset(dbptr);

  // maybe init data first
  {
    std::string value;
    std::string key(ENGINE_META_KEY_DIM);
    leveldb::Slice sls_key(key);
    status = db_->Get(leveldb::ReadOptions(), sls_key, &value);
    if (status.IsNotFound()) {
      char value_buf[sizeof(dim_)];
      vraft::EncodeFixed32(value_buf, dim_);
      leveldb::Slice sls_value(value_buf, sizeof(dim_));
      status = db_->Put(leveldb::WriteOptions(), sls_key, sls_value);
      assert(status.ok());
    } else {
      assert(status.ok());
      assert(value.size() == sizeof(dim_));
      dim_ = vraft::DecodeFixed32(value.c_str());
    }
  }
}

void EngineMeta::PersistDim() {
  std::string key(ENGINE_META_KEY_DIM);
  leveldb::Slice sls_key(key);
  char value_buf[sizeof(dim_)];
  vraft::EncodeFixed32(value_buf, dim_);
  leveldb::Slice sls_value(value_buf, sizeof(dim_));
  leveldb::Status status =
      db_->Put(leveldb::WriteOptions(), sls_key, sls_value);
  assert(status.ok());
}

}  // namespace vectordb
