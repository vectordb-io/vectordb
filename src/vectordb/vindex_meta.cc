#include "vindex_meta.h"

#include "allocator.h"
#include "common.h"
#include "util.h"
#include "vraft_logger.h"

namespace vectordb {

VindexMeta::VindexMeta(const std::string &path, VIndexParam &param)
    : path_(path), param_(param) {
  Init();
}

nlohmann::json VindexMeta::ToJson() {
  nlohmann::json j;
  j["param"] = param_.ToJson();
  return j;
}

nlohmann::json VindexMeta::ToJsonTiny() { return ToJson(); }

std::string VindexMeta::ToJsonString(bool tiny, bool one_line) {
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

void VindexMeta::Init() {
  bool need_init = false;
  if (!vraft::IsDirExist(path_)) {
    need_init = true;
  }

  int32_t rv = CreateDB();
  assert(rv == 0);

  if (need_init) {
    // write param from db
    Persist();

  } else {
    // read param from db
    std::string key(VINDEX_META_KEY);
    leveldb::Slice sls_key(key);

    std::string value;
    auto s = db_->Get(leveldb::ReadOptions(), sls_key, &value);

    VIndexParam tmp_param;
    int32_t bytes = tmp_param.FromString(value);
    assert(bytes > 0);

    param_ = tmp_param;
  }
}

void VindexMeta::Persist() {
  std::string key(VINDEX_META_KEY);
  leveldb::Slice sls_key(key);
  std::string value;
  param_.ToString(value);
  leveldb::Slice sls_value(value);
  auto s = db_->Put(leveldb::WriteOptions(), sls_key, sls_value);
  assert(s.ok());
}

int32_t VindexMeta::CreateDB() {
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
  return 0;
}

}  // namespace vectordb
