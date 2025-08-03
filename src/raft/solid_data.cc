#include "solid_data.h"

#include "coding.h"
#include "common.h"
#include "raft_addr.h"
#include "vraft_logger.h"

namespace vraft {

SolidData::SolidData(const std::string &path) : path_(path) {
  vraft_logger.FInfo("solid-data construct, path:%s, %p", path_.c_str(), this);
}

SolidData::~SolidData() {
  db_.reset();
  vraft_logger.FInfo("solid-data destruct, path:%s, %p", path_.c_str(), this);
}

void SolidData::Init() {
  db_options_.create_if_missing = true;
  db_options_.error_if_exists = false;
  leveldb::DB *dbptr;
  leveldb::Status status = leveldb::DB::Open(db_options_, path_, &dbptr);
  if (!status.ok()) {
    vraft_logger.FError("leveldb open %s error, %s", path_.c_str(),
                        status.ToString().c_str());
    assert(0);
  }
  db_.reset(dbptr);

  // maybe init first
  {
    std::string value;
    leveldb::Slice sls_key(reinterpret_cast<const char *>(&kTermKey),
                           sizeof(kTermKey));
    status = db_->Get(leveldb::ReadOptions(), sls_key, &value);
    if (status.IsNotFound()) {
      term_ = 1;
      char value_buf[sizeof(term_)];
      EncodeFixed64(value_buf, term_);
      leveldb::Slice sls_value(value_buf, sizeof(term_));
      status = db_->Put(leveldb::WriteOptions(), sls_key, sls_value);
      assert(status.ok());
    } else {
      assert(status.ok());
      assert(value.size() == sizeof(term_));
      term_ = DecodeFixed64(value.c_str());
    }
  }

  // maybe init first
  {
    std::string value;
    leveldb::Slice sls_key(reinterpret_cast<const char *>(&kVoteKey),
                           sizeof(kVoteKey));
    status = db_->Get(leveldb::ReadOptions(), sls_key, &value);
    if (status.IsNotFound()) {
      vote_ = 0;
      char value_buf[sizeof(vote_)];
      EncodeFixed64(value_buf, vote_);
      leveldb::Slice sls_value(value_buf, sizeof(vote_));
      status = db_->Put(leveldb::WriteOptions(), sls_key, sls_value);
      assert(status.ok());
    } else {
      assert(value.size() == sizeof(vote_));
      vote_ = DecodeFixed64(value.c_str());
    }
  }
}

void SolidData::IncrTerm() {
  ++term_;
  PersistTerm();
}

void SolidData::SetTerm(uint64_t term) {
  term_ = term;
  PersistTerm();
}

void SolidData::SetVote(uint64_t vote) {
  vote_ = vote;
  PersistVote();
}

void SolidData::PersistTerm() {
  leveldb::Slice sls_key(reinterpret_cast<const char *>(&kTermKey),
                         sizeof(kTermKey));
  char value_buf[sizeof(term_)];
  EncodeFixed64(value_buf, term_);
  leveldb::Slice sls_value(value_buf, sizeof(term_));
  leveldb::Status status =
      db_->Put(leveldb::WriteOptions(), sls_key, sls_value);
  assert(status.ok());
}

void SolidData::PersistVote() {
  leveldb::Slice sls_key(reinterpret_cast<const char *>(&kVoteKey),
                         sizeof(kVoteKey));
  char value_buf[sizeof(vote_)];
  EncodeFixed64(value_buf, vote_);
  leveldb::Slice sls_value(value_buf, sizeof(vote_));
  leveldb::Status status =
      db_->Put(leveldb::WriteOptions(), sls_key, sls_value);
  assert(status.ok());
}

nlohmann::json SolidData::ToJson() {
  nlohmann::json j;
  j["term"] = term_;
  if (vote_ == 0) {
    j["vote"] = "0";
  } else {
    RaftAddr addr(vote_);
    j["vote"] = addr.ToString();
  }
  return j;
}

nlohmann::json SolidData::ToJsonTiny() {
  nlohmann::json j;
  j["tm"] = term_;
  if (vote_ == 0) {
    j["vt"] = "0";
  } else {
    RaftAddr addr(vote_);
    j["vt"] = addr.ToString();
  }
  return j;
}

std::string SolidData::ToJsonString(bool tiny, bool one_line) {
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

}  // namespace vraft
