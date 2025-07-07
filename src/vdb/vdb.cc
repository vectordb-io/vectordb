#include "vdb.h"

#include <cassert>
#include <fstream>

#include "logger.h"
#include "util.h"

namespace vectordb {

const std::string kVersion = "0.0.1";

Vdb::Vdb(const vdb::DBParam &param) : param_(param) { Init(); }

Vdb::~Vdb() { DestroyLogger(); }

std::string Vdb::Version() const { return kVersion; }

void Vdb::Init() {
  RetNo ret = RET_OK;
  if (fs::exists(param_.path())) {
    ret = Load();
  } else {
    ret = New();
  }

  if (ret != RET_OK) {
    logger->error("init vdb failed, ret: {}", RetNoToString(ret));
    assert(0);
  }
}

RetNo Vdb::New() {
  Prepare();
  InitLogger(param_.path() + "/vdb.log");

  logger->info("new vdb ok");
  return RET_OK;
}

RetNo Vdb::Load() {
  InitLogger(param_.path() + "/vdb.log");

  // 加载表
  for (const auto &table_param : param_.tables()) {
    std::string table_name = table_param.name();
    if (tables_.find(table_name) != tables_.end()) {
      logger->warn("table {} already exists", table_name);
      continue;
    }

    TableSPtr table = std::make_shared<Table>(table_param);
    if (table == nullptr) {
      logger->error("load table {} failed", table_name);
      return RET_ERROR;
    }

    tables_[table_name] = table;
    logger->info("load table {} success", table_name);
  }

  logger->info("load vdb ok");

  return RET_OK;
}

void Vdb::Prepare() {
  assert(!fs::exists(param_.path()));
  fs::create_directories(param_.path());
}

RetNo Vdb::CreateTable(const std::string &name, int32_t dim) {
  vdb::IndexInfo index_info;
  index_info.set_index_type(vectordb::kDefaultIndexType);
  vdb::HnswParam hnsw_param = vectordb::DefaultHnswParam(dim);
  index_info.mutable_hnsw_param()->CopyFrom(hnsw_param);
  return CreateTable(name, index_info);
}

RetNo Vdb::CreateTable(const std::string &name,
                       const vdb::IndexInfo &default_index_info) {
  vdb::TableParam param;
  param.set_path(param_.path() + "/" + name);
  param.set_name(name);
  param.set_create_time(TimeStamp().MilliSeconds());

  int32_t dim = 0;
  switch (default_index_info.index_type()) {
    case INDEX_TYPE_FLAT:
      dim = default_index_info.flat_param().dim();
      break;
    case INDEX_TYPE_HNSW:
      dim = default_index_info.hnsw_param().dim();
      break;
    default:
      return RET_ERROR;
  }
  param.set_dim(dim);
  param.mutable_default_index_info()->CopyFrom(default_index_info);

  if (tables_.find(param.name()) != tables_.end()) {
    logger->warn("table {} already exists", param.name());
    return RET_ERROR;
  }

  TableSPtr table = std::make_shared<Table>(param);
  if (table == nullptr) {
    logger->error("create table {} failed", param.name());
    return RET_ERROR;
  }

  tables_[param.name()] = table;
  logger->info("create table {} success", param.name());

  vdb::TableParam *new_param = param_.mutable_tables()->Add();
  new_param->CopyFrom(param);
  return RET_OK;
}

RetNo Vdb::DropTable(const std::string &name, bool delete_data) {
  // 检查表是否存在
  auto it = tables_.find(name);
  if (it == tables_.end()) {
    logger->warn("table {} not found", name);
    return RET_NOT_FOUND;
  }

  // 获取表对象
  TableSPtr table = it->second;

  // 从映射中移除表
  tables_.erase(it);
  logger->info("table {} dropped from memory", name);

  // 如果需要删除数据
  if (delete_data) {
    // 获取表参数中的路径
    std::string table_path = table->param().path();
    if (!table_path.empty() && fs::exists(table_path)) {
      try {
        fs::remove_all(table_path);
        logger->info("table {} data deleted from disk: {}", name, table_path);
      } catch (const fs::filesystem_error &e) {
        logger->error("failed to delete table {} data: {}", name, e.what());
        return RET_ERROR;
      }
    }
  }

  return RET_OK;
}

TableSPtr Vdb::GetTable(const std::string &name) {
  auto it = tables_.find(name);
  if (it == tables_.end()) {
    return nullptr;
  }
  return it->second;
}

RetNo Vdb::Add(const std::string &table_name, int64_t id,
               std::vector<float> &vector, const std::string &scalar,
               const WOptions &options, bool normalize) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    return RET_ERROR;
  }
  return table->Add(id, vector, scalar, options, normalize);
}

RetNo Vdb::Add(const std::string &table_name, int64_t id,
               std::vector<float> &vector, const WOptions &options,
               bool normalize) {
  return Add(table_name, id, vector, "", options, normalize);
}

RetNo Vdb::Get(const std::string &table_name, int64_t id,
               std::vector<float> &vector, std::string &scalar) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }

  return table->Get(id, vector, scalar);
}

RetNo Vdb::Get(const std::string &table_name, int64_t id,
               std::vector<float> &vector) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }

  return table->Get(id, vector);
}

RetNo Vdb::Get(const std::string &table_name, int64_t id, std::string &scalar) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }

  return table->Get(id, scalar);
}

RetNo Vdb::Search(const std::string &table_name, const std::vector<float> &v,
                  int32_t k, std::vector<int64_t> &ids,
                  std::vector<float> &distances,
                  std::vector<std::string> &scalars, const ROptions &options,
                  int32_t index_id) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }

  // 调用表的搜索方法，传递索引ID
  return table->Search(v, k, ids, distances, scalars, options, index_id);
}

RetNo Vdb::Search(const std::string &table_name, int64_t id, int32_t k,
                  std::vector<int64_t> &ids, std::vector<float> &distances,
                  std::vector<std::string> &scalars, const ROptions &options,
                  int32_t index_id) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }

  // 调用表的基于ID的搜索方法，传递索引ID
  return table->Search(id, k, ids, distances, scalars, options, index_id);
}

RetNo Vdb::BuildIndex(const std::string &table_name) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }

  return table->BuildIndex();
}

RetNo Vdb::BuildIndex(const std::string &table_name,
                      const vdb::IndexInfo &param) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }

  // 根据索引类型选择不同的构建方法
  RetNo ret = RET_ERROR;
  switch (param.index_type()) {
    case INDEX_TYPE_FLAT: {
      ret = table->BuildIndex(param.flat_param());
      break;
    }
    case INDEX_TYPE_HNSW: {
      ret = table->BuildIndex(param.hnsw_param());
      break;
    }
    default: {
      logger->error("invalid index type: {}", param.index_type());
      return RET_ERROR;
    }
  }

  return ret;
}

// the newest 'left' indexes will be kept
RetNo Vdb::DropIndex(const std::string &table_name, int32_t left) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }

  RetNo ret = table->DropIndex(left);
  if (ret == RET_OK) {
    logger->info("drop index for table {} success, keeping newest {} indexes",
                 table_name, left);
  } else {
    logger->error("drop index for table {} failed, ret: {}", table_name,
                  RetNoToString(ret));
  }

  return ret;
}

std::string Vdb::MetaStr() const {
  std::string s;
  param_.SerializeToString(&s);
  return s;
}

std::vector<int32_t> Vdb::IndexIDs(const std::string &table_name) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return {};
  }
  return table->IndexIDs();
}

RetNo Vdb::Persist() {
  for (const auto &table : tables_) {
    table.second->Persist();
  }
  return RET_OK;
}

RetNo Vdb::Persist(const std::string &table_name) {
  TableSPtr table = GetTable(table_name);
  if (table == nullptr) {
    logger->warn("table {} not found", table_name);
    return RET_NOT_FOUND;
  }
  return table->Persist();
}

}  // namespace vectordb