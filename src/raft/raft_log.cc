#include "raft_log.h"

#include <cassert>

#include "coding.h"
#include "leveldb/write_batch.h"
#include "tracer.h"

namespace vraft {

// log_index: 1 2 3
// term_key : 1 3 5
// value_key: 2 4 6
RaftIndex LogIndexToMetaIndex(RaftIndex log_index) { return log_index * 2 - 1; }

RaftIndex MetaIndexToLogIndex(RaftIndex term_index) {
  assert(term_index % 2 == 1);
  return (term_index + 1) / 2;
}

RaftIndex LogIndexToDataIndex(RaftIndex log_index) { return log_index * 2; }

RaftIndex DataIndexToLogIndex(RaftIndex value_index) {
  assert(value_index % 2 == 0);
  return value_index / 2;
}

void EncodeDataKey(char *buf, int32_t len, RaftIndex log_index) {
  assert(len >= static_cast<int32_t>(sizeof(RaftIndex)));
  RaftIndex value_key = LogIndexToDataIndex(log_index);
  EncodeFixed32(buf, value_key);
}

void EncodeMetaKey(char *buf, int32_t len, RaftIndex log_index) {
  assert(len >= static_cast<int32_t>(sizeof(RaftIndex)));
  RaftIndex term_index = LogIndexToMetaIndex(log_index);
  EncodeFixed32(buf, term_index);
}
int32_t MetaValueBytes() { return sizeof(RaftTerm) + 4 * sizeof(uint32_t); }

void EncodeMetaValue(char *buf, int32_t len, MetaValue &meta) {
  assert(len >= MetaValueBytes());
  char *p = buf;
  EncodeFixed64(p, meta.term);
  p += sizeof(meta.term);

  uint32_t u32 = meta.type;
  EncodeFixed32(p, u32);
  p += sizeof(u32);

  EncodeFixed32(p, meta.pre_chk_all);
  p += sizeof(meta.pre_chk_all);

  EncodeFixed32(p, meta.chk_ths);
  p += sizeof(meta.chk_ths);

  EncodeFixed32(p, meta.chk_all);
  p += sizeof(meta.chk_all);
}

void DecodeMetaValue(const char *buf, int32_t len, MetaValue &meta) {
  char *p = const_cast<char *>(buf);
  uint32_t u32 = 0;

  meta.term = DecodeFixed64(p);
  p += sizeof(meta.term);

  u32 = DecodeFixed32(p);
  p += sizeof(u32);
  meta.type = U32ToEntryType(u32);

  meta.pre_chk_all = DecodeFixed32(p);
  p += sizeof(meta.pre_chk_all);

  meta.chk_ths = DecodeFixed32(p);
  p += sizeof(meta.chk_ths);

  meta.chk_all = DecodeFixed32(p);
  p += sizeof(meta.chk_all);
}

void EncodeZeroValue(char *buf, RaftIndex append, uint32_t checksum) {
  char *p = buf;
  EncodeFixed32(p, append);
  p += sizeof(append);

  EncodeFixed32(p, checksum);
  p += sizeof(checksum);
}

void DecodeZeroValue(const char *buf, RaftIndex &append, uint32_t &checksum) {
  char *p = const_cast<char *>(buf);

  append = DecodeFixed32(p);
  p += sizeof(append);

  checksum = DecodeFixed32(p);
  p += sizeof(checksum);
}

const leveldb::Comparator *U32Comparator() {
  static NoDestructor<U32ComparatorImpl> singleton;
  return singleton.get();
}

U32ComparatorImpl::~U32ComparatorImpl() {}

int U32ComparatorImpl::Compare(const leveldb::Slice &a,
                               const leveldb::Slice &b) const {
  assert(a.size() == sizeof(uint32_t));
  assert(b.size() == sizeof(uint32_t));

  uint32_t da = DecodeFixed32(a.data());
  uint32_t db = DecodeFixed32(b.data());
  int32_t diff = da - db;
  if (diff < 0) {
    return -1;
  } else if (diff > 0) {
    return 1;
  } else {
    return 0;
  }
}

void U32ComparatorImpl::FindShortestSeparator(
    std::string *start, const leveldb::Slice &limit) const {
  // do nothing, just make no warning
  if (start->size() == 0 || limit.size() == 0) return;
}

void U32ComparatorImpl::FindShortSuccessor(std::string *key) const {
  // do nothing, just make no warning
  if (key->size() == 0) return;
}

// index    : 1 2 3
// term_key : 1 3 5
// value_key: 2 4 6
void RaftLog::Init() {
  db_options_.create_if_missing = true;
  db_options_.error_if_exists = false;
  db_options_.comparator = U32Comparator();
  leveldb::DB *dbptr;
  leveldb::Status status = leveldb::DB::Open(db_options_, path_, &dbptr);
  std::string err_str = status.ToString();
  assert(status.ok());
  db_.reset(dbptr);

  leveldb::ReadOptions ro;
  leveldb::Iterator *it = db_->NewIterator(ro);

  // maybe init first
  it->SeekToFirst();
  if (!it->Valid()) {
    leveldb::WriteBatch batch;
    char zero_value[sizeof(RaftIndex) + sizeof(uint32_t)];
    EncodeZeroValue(zero_value, 1, 0);
    batch.Put(leveldb::Slice(kZeroKey, sizeof(kZeroKey)),
              leveldb::Slice(zero_value, sizeof(zero_value)));
    leveldb::WriteOptions wo;
    wo.sync = true;
    leveldb::Status s = db_->Write(wo, &batch);
    assert(s.ok());

    first_ = 0;
    last_ = 0;
    append_ = 1;
  }
  delete it;

  // init
  it = db_->NewIterator(ro);
  it->SeekToLast();
  assert(it->Valid());
  assert(it->key().ToString().size() == sizeof(RaftIndex));
  RaftIndex index = DecodeFixed32(it->key().ToString().c_str());

  if (index == 0) {
    // no value
    first_ = 0;
    last_ = 0;

    // get zero value
    assert(it->value().ToString().size() ==
           sizeof(RaftIndex) + sizeof(uint32_t));

    // append, checksum
    DecodeZeroValue(it->value().ToString().c_str(), append_, last_checksum_);

  } else {  // has value
    // last, data index
    RaftIndex data_index = DecodeFixed32(it->key().ToString().c_str());
    last_ = DataIndexToLogIndex(data_index);

    // first, meta index
    it->SeekToFirst();
    it->Next();
    RaftIndex meta_index = DecodeFixed32(it->key().ToString().c_str());
    first_ = MetaIndexToLogIndex(meta_index);

    // append
    append_ = last_ + 1;

    // checksum
    MetaValuePtr ptr = LastMeta();
    assert(ptr);
    last_checksum_ = ptr->chk_all;
  }

  delete it;
  Check();
}

void RaftLog::Check() {
  if (first_ == last_) {
    if (first_ == 0) {
      // no value
      assert(append_ >= 1);
    }

    if (first_ > 0) {
      // has value
      assert(append_ == last_ + 1);
    }

  } else {
    // has value
    assert(first_ <= last_);
    assert(first_ >= 1);
    assert(append_ == last_ + 1);
  }
}

bool RaftLog::IndexValid(RaftIndex index) {
  if (index == 0) {
    return false;
  }

  if (first_ <= index && index <= last_) {
    return true;

  } else {
    return false;
  }
}

RaftLog::RaftLog(const std::string &path)
    : first_(0),
      last_(0),
      append_(0),
      checksum_(true),
      last_checksum_(0),
      path_(path) {}

int32_t RaftLog::GetMeta(RaftIndex index, MetaValue &meta) {
  Check();

  char meta_key[sizeof(RaftIndex)];
  EncodeMetaKey(meta_key, sizeof(RaftIndex), index);
  leveldb::ReadOptions ro;
  leveldb::Status s;
  std::string meta_value;
  s = db_->Get(ro, leveldb::Slice(meta_key, sizeof(meta_key)), &meta_value);
  if (s.ok()) {
    assert(static_cast<int32_t>(meta_value.size()) == MetaValueBytes());
    DecodeMetaValue(meta_value.c_str(), meta_value.size(), meta);
    return 0;

  } else {
    return -1;
  }
}

int32_t RaftLog::GetValue(RaftIndex index, std::string *value) {
  Check();

  char data_key[sizeof(RaftIndex)];
  EncodeDataKey(data_key, sizeof(RaftIndex), index);
  leveldb::ReadOptions ro;
  leveldb::Status s;
  s = db_->Get(ro, leveldb::Slice(data_key, sizeof(data_key)), value);
  if (s.ok()) {
    return 0;
  } else {
    return -1;
  }
}

int32_t RaftLog::Get(RaftIndex index, LogEntry &entry) {
  Check();

  MetaValue meta;
  int32_t rv = GetMeta(index, meta);
  if (rv == 0) {
    rv = GetValue(index, &(entry.append_entry.value));
    assert(rv == 0);
    entry.index = index;
    entry.pre_chk_all = meta.pre_chk_all;
    entry.chk_ths = meta.chk_ths;
    entry.chk_all = meta.chk_all;
    entry.append_entry.term = meta.term;
    entry.append_entry.type = meta.type;
    return 0;

  } else {
    return -1;
  }
}

LogEntryPtr RaftLog::LastEntry() {
  Check();
  if (last_ == 0) {
    return nullptr;

  } else {
    LogEntryPtr ptr = std::make_shared<LogEntry>();
    int32_t rv = Get(last_, *(ptr.get()));
    assert(rv == 0);
    return ptr;
  }
}

MetaValuePtr RaftLog::LastMeta() {
  Check();
  if (last_ == 0) {
    return nullptr;

  } else {
    MetaValuePtr ptr = std::make_shared<MetaValue>();
    int32_t rv = GetMeta(last_, *(ptr.get()));
    assert(rv == 0);
    return ptr;
  }
}

int32_t RaftLog::AppendOne(AppendEntry &entry, Tracer *tracer) {
  Check();
  RaftIndex tmp_first, tmp_last, tmp_append;
  uint32_t tmp_checksum = 0;

  if (first_ == last_ && first_ == 0) {
    // no value
    tmp_first = append_;
    tmp_last = append_;
    tmp_append = append_ + 1;

  } else {
    // has value
    // do not need to persist append index every time

    tmp_first = first_;
    tmp_last = last_ + 1;
    tmp_append = tmp_last + 1;
  }

  leveldb::WriteBatch batch;
  RaftIndex log_index = tmp_last;

  char meta_key[sizeof(RaftIndex)];
  EncodeMetaKey(meta_key, sizeof(RaftIndex), log_index);

  char meta_value[sizeof(MetaValue)];
  if (checksum_) {
    LogEntry log_entry;
    log_entry.index = append_;
    log_entry.append_entry = entry;

    log_entry.pre_chk_all = last_checksum_;
    log_entry.CheckSum();

    // save tmp_checksum
    tmp_checksum = log_entry.chk_all;

    MetaValue mv;
    mv.term = entry.term;
    mv.type = entry.type;
    mv.pre_chk_all = log_entry.pre_chk_all;
    mv.chk_ths = log_entry.chk_ths;
    mv.chk_all = log_entry.chk_all;
    EncodeMetaValue(meta_value, sizeof(MetaValue), mv);

  } else {
    MetaValue mv;
    mv.term = entry.term;
    mv.type = entry.type;
    mv.pre_chk_all = 0;
    mv.chk_ths = 0;
    mv.chk_all = 0;
    EncodeMetaValue(meta_value, sizeof(MetaValue), mv);
  }

  batch.Put(leveldb::Slice(meta_key, sizeof(meta_key)),
            leveldb::Slice(meta_value, sizeof(meta_value)));

  char data_key[sizeof(RaftIndex)];
  EncodeDataKey(data_key, sizeof(RaftIndex), log_index);
  batch.Put(leveldb::Slice(data_key, sizeof(data_key)),
            leveldb::Slice(entry.value.c_str(), entry.value.size()));

  leveldb::WriteOptions wo;
  wo.sync = true;
  leveldb::Status s = db_->Write(wo, &batch);
  assert(s.ok());

  if (tracer) {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "append one log, term:%lu, index:%u, type:%s, value-len:%lu",
             entry.term, append_, EntryTypeToStr(entry.type),
             entry.value.size());
    tracer->PrepareEvent(kEventOther, std::string(buf));
  }

  // update index after persist value
  // do not need to persist append index every time
  first_ = tmp_first;
  last_ = tmp_last;
  append_ = tmp_append;

  // update last_checksum_
  last_checksum_ = tmp_checksum;

  Check();
  return 0;
}

int32_t RaftLog::AppendSome(std::vector<AppendEntry> &entries) { return 0; }

int32_t RaftLog::DeleteFrom(RaftIndex from_index) {
  Check();
  RaftIndex tmp_first, tmp_last, tmp_append;
  uint32_t tmp_checksum = 0;

  // no value
  if (first_ == last_ && first_ == 0) {
    return 0;
  }

  // has value
  assert(first_ > 0);
  assert(last_ > 0);
  assert(last_ >= first_);
  assert(append_ == last_ + 1);

  // adjust from index
  if (from_index > last_) {
    return 0;
  }

  // adjust from index
  if (from_index < first_) {
    from_index = first_;
  }
  assert(first_ <= from_index);
  assert(from_index <= last_);

  // save update index
  if (from_index == first_) {
    tmp_first = 0;
    tmp_last = 0;
    tmp_append = from_index;

  } else {
    assert(from_index > first_);
    tmp_first = first_;
    tmp_last = from_index - 1;
    tmp_append = from_index;
  }

  // update checksum
  assert(from_index >= 1);
  MetaValue meta;
  int32_t rv = GetMeta(from_index, meta);
  assert(rv == 0);
  tmp_checksum = meta.pre_chk_all;

  leveldb::WriteBatch batch;
  for (RaftIndex i = from_index; i <= last_; ++i) {
    char meta_key[sizeof(RaftIndex)];
    EncodeMetaKey(meta_key, sizeof(RaftIndex), i);
    batch.Delete(leveldb::Slice(meta_key, sizeof(meta_key)));

    char data_key[sizeof(RaftIndex)];
    EncodeDataKey(data_key, sizeof(RaftIndex), i);
    batch.Delete(leveldb::Slice(data_key, sizeof(data_key)));
  }

  // need to persist append index
  if (from_index == first_) {
    char zero_value[sizeof(RaftIndex) + sizeof(uint32_t)];
    EncodeZeroValue(zero_value, tmp_append, tmp_checksum);
    batch.Put(leveldb::Slice(kZeroKey, sizeof(kZeroKey)),
              leveldb::Slice(zero_value, sizeof(zero_value)));
  }

  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  assert(s.ok());

  // update index
  first_ = tmp_first;
  last_ = tmp_last;
  append_ = tmp_append;
  last_checksum_ = tmp_checksum;

  Check();
  return 0;
}

int32_t RaftLog::DeleteUtil(RaftIndex to_index) {
  Check();
  RaftIndex tmp_first, tmp_last, tmp_append;
  uint32_t tmp_checksum = 0;

  // no value
  if (first_ == last_ && first_ == 0) {
    return 0;
  }

  // has value
  assert(first_ > 0);
  assert(last_ > 0);
  assert(last_ >= first_);
  assert(append_ == last_ + 1);

  // adjust to index
  if (to_index < first_) {
    return 0;
  }

  // adjust to index
  if (to_index > last_) {
    to_index = last_;
  }
  assert(first_ <= to_index);
  assert(to_index <= last_);

  // save update index
  if (to_index == last_) {
    tmp_first = 0;
    tmp_last = 0;
    tmp_append = last_ + 1;

  } else {
    assert(to_index < last_);
    tmp_first = to_index + 1;
    tmp_last = last_;
    tmp_append = last_ + 1;
  }

  // update checksum
  if (to_index < last_) {
    tmp_checksum = last_checksum_;

  } else {
    assert(to_index == last_);
    MetaValue meta;
    int32_t rv = GetMeta(to_index, meta);
    assert(rv == 0);
    tmp_checksum = meta.chk_all;
  }

  leveldb::WriteBatch batch;
  for (RaftIndex i = first_; i <= to_index; ++i) {
    char meta_key[sizeof(RaftIndex)];
    EncodeMetaKey(meta_key, sizeof(RaftIndex), i);
    batch.Delete(leveldb::Slice(meta_key, sizeof(meta_key)));

    char data_key[sizeof(RaftIndex)];
    EncodeDataKey(data_key, sizeof(RaftIndex), i);
    batch.Delete(leveldb::Slice(data_key, sizeof(data_key)));
  }

  // need to persist append index
  if (to_index == last_) {
    char zero_value[sizeof(RaftIndex) + sizeof(uint32_t)];
    EncodeZeroValue(zero_value, tmp_append, tmp_checksum);
    batch.Put(leveldb::Slice(kZeroKey, sizeof(kZeroKey)),
              leveldb::Slice(zero_value, sizeof(zero_value)));
  }

  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  assert(s.ok());

  // update index
  first_ = tmp_first;
  last_ = tmp_last;
  append_ = tmp_append;
  last_checksum_ = tmp_checksum;

  Check();
  return 0;
}

}  // namespace vraft
