#include "vectordb.h"

#include "logger.h"
#include "util.h"

namespace vectordb {

const std::string kMetaKey = "meta";

Vectordb::Vectordb(const std::string &name, const std::string &path)
    : name_(name), path_(path) {
  data_path_ = path_ + "/data";
  meta_path_ = path_ + "/meta";
  log_path_ = path_ + "/log";

  Init();
}

Vectordb::~Vectordb() { DestroyLogger(); }

void Vectordb::Init() {
  RetNo ret = RET_OK;
  if (fs::exists(path_)) {
    ret = Load();
  } else {
    ret = New();
  }

  if (ret != RET_OK) {
    logger->error("init vectordb failed, ret: {}", RetNoToString(ret));
    assert(0);
  }
}

RetNo Vectordb::New() {
  Prepare();
  InitLogger(log_path_ + "/vectordb.log");

  // create data
  vdb::DBParam param;
  param.set_path(data_path_);
  param.set_name(name_);
  param.set_create_time(TimeStamp().MilliSeconds());
  vdb_ = std::make_shared<Vdb>(param);
  vdb_->Persist();

  // create meta
  rocksdb::Options options;
  options.create_if_missing = true;
  options.error_if_exists = true;

  rocksdb::DB *db_ptr = nullptr;
  rocksdb::Status status = rocksdb::DB::Open(options, meta_path_, &db_ptr);
  if (!status.ok()) {
    logger->error("open meta db failed, ret: {}", status.ToString());
    return RET_ERROR;
  }
  meta_.reset(db_ptr);

  // persist meta
  PersistMeta();

  logger->info("new vectordb ok");
  return RET_OK;
}

RetNo Vectordb::Load() {
  InitLogger(log_path_ + "/vectordb.log");

  vdb::DBParam param = LoadMeta();
  vdb_ = std::make_shared<Vdb>(param);

  logger->info("load vectordb ok");
  return RET_OK;
}

void Vectordb::Prepare() {
  assert(!fs::exists(path_));
  fs::create_directories(path_);
  fs::create_directories(log_path_);
}

RetNo Vectordb::PersistMeta() {
  std::string meta_str = vdb_->MetaStr();
  rocksdb::Status status =
      meta_->Put(rocksdb::WriteOptions(), kMetaKey, meta_str);
  if (!status.ok()) {
    logger->error("persist meta failed, ret: {}", status.ToString());
    return RET_ERROR;
  }

  return RET_OK;
}

vdb::DBParam Vectordb::LoadMeta() {
  rocksdb::Options options;
  options.create_if_missing = false;
  options.error_if_exists = false;

  rocksdb::DB *db_ptr = nullptr;
  rocksdb::Status status = rocksdb::DB::Open(options, meta_path_, &db_ptr);
  if (!status.ok()) {
    logger->error("open meta db failed, ret: {}", status.ToString());
    return vdb::DBParam();
  }
  meta_.reset(db_ptr);

  std::string meta_str;
  status = meta_->Get(rocksdb::ReadOptions(), kMetaKey, &meta_str);
  if (!status.ok()) {
    logger->error("load meta failed, ret: {}", status.ToString());
    return vdb::DBParam();
  }

  vdb::DBParam param;
  param.ParseFromString(meta_str);
  return param;
}

RetNo Vectordb::CreateTable(const std::string &name, int32_t dim) {
  RetNo ret = vdb_->CreateTable(name, dim);
  if (ret != RET_OK) {
    logger->error("create table failed, ret: {}", RetNoToString(ret));
    return ret;
  }
  return PersistMeta();
}

RetNo Vectordb::CreateTable(const std::string &name,
                            const vdb::IndexInfo &default_index_info) {
  RetNo ret = vdb_->CreateTable(name, default_index_info);
  if (ret != RET_OK) {
    logger->error("create table failed, ret: {}", RetNoToString(ret));
    return ret;
  }
  return PersistMeta();
}

RetNo Vectordb::DropTable(const std::string &name, bool delete_data) {
  RetNo ret = vdb_->DropTable(name, delete_data);
  if (ret != RET_OK) {
    logger->error("drop table failed, ret: {}", RetNoToString(ret));
    return ret;
  }
  return PersistMeta();
}

RetNo Vectordb::Add(const std::string &table_name, int64_t id,
                    std::vector<float> &vector, const std::string &scalar,
                    const WOptions &options, bool normalize) {
  return vdb_->Add(table_name, id, vector, scalar, options, normalize);
}

RetNo Vectordb::Add(const std::string &table_name, int64_t id,
                    std::vector<float> &vector, const WOptions &options,
                    bool normalize) {
  return vdb_->Add(table_name, id, vector, options, normalize);
}

// input: id
// output: vector, scalar
RetNo Vectordb::Get(const std::string &table_name, int64_t id,
                    std::vector<float> &vector, std::string &scalar) {
  return vdb_->Get(table_name, id, vector, scalar);
}

// input: id
// output: vector
RetNo Vectordb::Get(const std::string &table_name, int64_t id,
                    std::vector<float> &vector) {
  return vdb_->Get(table_name, id, vector);
}

// input: id
// output: scalar
RetNo Vectordb::Get(const std::string &table_name, int64_t id,
                    std::string &scalar) {
  return vdb_->Get(table_name, id, scalar);
}

// input: v, k
// output: ids, distances, scalars
// index_id: -1 means the newest index
RetNo Vectordb::Search(const std::string &table_name,
                       const std::vector<float> &v, int32_t k,
                       std::vector<int64_t> &ids, std::vector<float> &distances,
                       std::vector<std::string> &scalars,
                       const ROptions &options, int32_t index_id) {
  return vdb_->Search(table_name, v, k, ids, distances, scalars, options,
                      index_id);
}

// input: id, k
// output: ids, distances, scalars
// index_id: -1 means the newest index
RetNo Vectordb::Search(const std::string &table_name, int64_t id, int32_t k,
                       std::vector<int64_t> &ids, std::vector<float> &distances,
                       std::vector<std::string> &scalars,
                       const ROptions &options, int32_t index_id) {
  return vdb_->Search(table_name, id, k, ids, distances, scalars, options,
                      index_id);
}

RetNo Vectordb::BuildIndex(const std::string &table_name) {
  RetNo ret = vdb_->BuildIndex(table_name);
  if (ret != RET_OK) {
    logger->error("build index failed, ret: {}", RetNoToString(ret));
    return ret;
  }
  return PersistMeta();
}

RetNo Vectordb::BuildIndex(const std::string &table_name,
                           const vdb::IndexInfo &param) {
  RetNo ret = vdb_->BuildIndex(table_name, param);
  if (ret != RET_OK) {
    logger->error("build index failed, ret: {}", RetNoToString(ret));
    return ret;
  }
  return PersistMeta();
}

// the newest 'left' indexes will be kept
RetNo Vectordb::DropIndex(const std::string &table_name, int32_t left) {
  RetNo ret = vdb_->DropIndex(table_name, left);
  if (ret != RET_OK) {
    logger->error("drop index failed, ret: {}", RetNoToString(ret));
    return ret;
  }
  return PersistMeta();
}

vdb::DBParam Vectordb::Meta() { return vdb_->Meta(); }

std::string Vectordb::MetaStr() const { return vdb_->MetaStr(); }

std::vector<int32_t> Vectordb::IndexIDs(const std::string &table_name) {
  return vdb_->IndexIDs(table_name);
}

RetNo Vectordb::Persist() { return vdb_->Persist(); }

RetNo Vectordb::Persist(const std::string &table_name) {
  return vdb_->Persist(table_name);
}

}  // namespace vectordb