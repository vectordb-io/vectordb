#include "keyid_meta.h"

#include "coding.h"
#include "util.h"
#include "vraft_logger.h"

namespace vectordb {

std::string EncodeAnnoyKey(const std::string &key) {
  std::string k(ANNOY_KEY_PREFIX);
  k.append(key);
  return k;
}

vraft::Slice DecodeAnnoyKey(const std::string &key) {
  size_t sz = key.size();
  size_t sz_prefix = std::string(ANNOY_KEY_PREFIX).size();
  assert(sz > sz_prefix);
  return vraft::Slice(key.c_str() + sz_prefix, sz - sz_prefix);
}

KeyidMeta::KeyidMeta(const std::string &path) : path_(path) { CreateDB(); }

int32_t KeyidMeta::Put(const std::string &vkey, int32_t id) {
  char value_buf[sizeof(id)];
  vraft::EncodeFixed32(value_buf, id);
  leveldb::Slice value(value_buf, sizeof(id));
  leveldb::WriteOptions wo;
  wo.sync = true;
  auto s = db_->Put(wo, vkey, value);
  assert(s.ok());
  return 0;
}

int32_t KeyidMeta::Put(int32_t id, const std::string &vkey) {
  char key_buf[sizeof(id)];
  vraft::EncodeFixed32(key_buf, id);
  leveldb::Slice key(key_buf, sizeof(id));
  std::string value = EncodeAnnoyKey(vkey);
  leveldb::WriteOptions wo;
  wo.sync = true;
  auto s = db_->Put(wo, key, value);
  assert(s.ok());
  return 0;
}

int32_t KeyidMeta::Get(const std::string &vkey, int32_t &id) {
  std::string value;
  auto s = db_->Get(leveldb::ReadOptions(), vkey, &value);
  if (s.ok()) {
    id = vraft::DecodeFixed32(value.c_str());
    return 0;

  } else if (s.IsNotFound()) {
    std::string hex_key = vraft::StrToHexStr(vkey.c_str(), vkey.size());
    vraft::vraft_logger.FInfo("key:%s value not found", hex_key.c_str());
    return -2;  // use Status instead!!
  }

  return -1;
}

int32_t KeyidMeta::Get(int32_t id, std::string &vkey) {
  char key_buf[sizeof(id)];
  vraft::EncodeFixed32(key_buf, id);
  leveldb::Slice key(key_buf, sizeof(id));

  std::string value;
  auto s = db_->Get(leveldb::ReadOptions(), key, &value);
  if (s.ok()) {
    vkey = DecodeAnnoyKey(value).ToString();
    return 0;

  } else if (s.IsNotFound()) {
    vraft::vraft_logger.FInfo("id:%d value not found", id);
    return -2;  // use Status instead!!
  }

  return -1;
}

int32_t KeyidMeta::CreateDB() {
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
