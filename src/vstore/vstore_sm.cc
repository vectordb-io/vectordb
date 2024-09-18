#include "vstore_sm.h"

#include "kv.h"
#include "leveldb/write_batch.h"
#include "raft_log.h"

namespace vstore {

vraft::StateMachineSPtr CreateVStoreSM(std::string &path) {
  vraft::StateMachineSPtr sptr(new VstoreSm(path));
  return sptr;
}

const std::string vs_last_index_key = "VSTORE_LAST_INDEX_KEY";
const std::string vs_last_term_key = "VSTORE_LAST_TERM_KEY";

VstoreSm::VstoreSm(std::string path) : StateMachine(path) {
  leveldb::Options o;
  o.create_if_missing = true;
  o.error_if_exists = false;
  leveldb::DB *db;
  leveldb::Status status = leveldb::DB::Open(o, path, &db);
  db_.reset(db);
  assert(status.ok());
}

int32_t VstoreSm::Restore() { return 0; }

int32_t VstoreSm::Apply(vraft::LogEntry *entry, vraft::RaftAddr addr) {
  leveldb::WriteBatch batch;

  {
    char buf[sizeof(uint32_t)];
    vraft::EncodeFixed32(buf, entry->index);
    batch.Put(leveldb::Slice(vs_last_index_key),
              leveldb::Slice(buf, sizeof(uint32_t)));
  }

  {
    char buf[sizeof(uint64_t)];
    vraft::EncodeFixed64(buf, entry->append_entry.term);
    batch.Put(leveldb::Slice(vs_last_term_key),
              leveldb::Slice(buf, sizeof(uint64_t)));
  }

#if 0
  std::vector<std::string> kv;
  vraft::Split(entry->append_entry.value, ':', kv);
  assert(kv.size() == 2);
  batch.Put(leveldb::Slice(kv[0]), leveldb::Slice(kv[1]));
#endif

  vraft::KV kv;
  kv.FromString(entry->append_entry.value);
  batch.Put(leveldb::Slice(kv.key), leveldb::Slice(kv.value));

  leveldb::WriteOptions wo;
  wo.sync = true;
  leveldb::Status s = db_->Write(wo, &batch);
  assert(s.ok());

  return 0;
}

vraft::RaftIndex VstoreSm::LastIndex() {
  leveldb::ReadOptions ro;
  leveldb::Status s;
  std::string value;
  s = db_->Get(ro, leveldb::Slice(vs_last_index_key), &value);
  if (s.ok()) {
    assert(value.size() == sizeof(uint32_t));
    uint32_t u32 = vraft::DecodeFixed32(value.c_str());
    return u32;
  } else {
    return 0;
  }
}

vraft::RaftTerm VstoreSm::LastTerm() {
  leveldb::ReadOptions ro;
  leveldb::Status s;
  std::string value;
  s = db_->Get(ro, leveldb::Slice(vs_last_term_key), &value);
  if (s.ok()) {
    assert(value.size() == sizeof(uint64_t));
    uint64_t u64 = vraft::DecodeFixed64(value.c_str());
    return u64;
  } else {
    return 0;
  }
}

int32_t VstoreSm::Get(const std::string &key, std::string &value) {
  leveldb::ReadOptions ro;
  leveldb::Status s;
  s = db_->Get(ro, leveldb::Slice(key), &value);
  if (s.ok()) {
    return 0;
  } else if (s.IsNotFound()) {
    return -2;
  } else {
    return -1;
  }
}

}  // namespace vstore
