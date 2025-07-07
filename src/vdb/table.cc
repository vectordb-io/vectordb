#include "table.h"

#include "common.h"
#include "distance.h"
#include "pb2json.h"
#include "util.h"

namespace vectordb {

const IndexType kDefaultIndexType = INDEX_TYPE_HNSW;
const DistanceType kDefaultDistanceType = DISTANCE_TYPE_INNER_PRODUCT;

const std::string kVectorColumnFamily = "vector";
const std::string kScalarColumnFamily = "scalar";

Table::Table(const vdb::TableParam &param)
    : data_path_(param.path() + "/data"),
      index_path_(param.path() + "/index"),
      description_file_(param.path() + "/description.json"),
      param_(param) {
  Init();
}

Table::~Table() {
  PersistDescription();

  // 释放所有的列族句柄
  for (auto &cf_pair : cf_handles_) {
    if (cf_pair.second != data_->DefaultColumnFamily()) {
      data_->DestroyColumnFamilyHandle(cf_pair.second);
    }
  }
  cf_handles_.clear();
}

void Table::Init() {
  RetNo ret = RET_OK;
  if (fs::exists(param_.path())) {
    ret = Load();
  } else {
    ret = New();
  }

  if (ret != RET_OK) {
    assert(0);
  }
}

RetNo Table::LoadIndex() {
  for (const auto &index_param : param_.indexes()) {
    VIndexSPtr index = std::make_shared<VIndex>(index_param);
    indexes_[index_param.id()] = index;
  }
  return RET_OK;
}

RetNo Table::NewData() {
  rocksdb::Options options;
  options.create_if_missing = true;
  options.error_if_exists = true;

  rocksdb::DB *db_ptr = nullptr;

  // 首先只用默认列族创建数据库
  rocksdb::Status status = rocksdb::DB::Open(options, data_path_, &db_ptr);
  if (!status.ok()) {
    return RET_ERROR;
  }

  data_.reset(db_ptr);

  // 获取默认列族句柄
  cf_handles_[rocksdb::kDefaultColumnFamilyName] =
      db_ptr->DefaultColumnFamily();

  // 创建其他列族
  rocksdb::ColumnFamilyHandle *vector_cf;
  status = db_ptr->CreateColumnFamily(rocksdb::ColumnFamilyOptions(),
                                      kVectorColumnFamily, &vector_cf);
  if (!status.ok()) {
    return RET_ERROR;
  }
  cf_handles_[kVectorColumnFamily] = vector_cf;

  rocksdb::ColumnFamilyHandle *scalar_cf;
  status = db_ptr->CreateColumnFamily(rocksdb::ColumnFamilyOptions(),
                                      kScalarColumnFamily, &scalar_cf);
  if (!status.ok()) {
    return RET_ERROR;
  }
  cf_handles_[kScalarColumnFamily] = scalar_cf;

  return RET_OK;
}

RetNo Table::LoadData() {
  rocksdb::Options options;
  options.create_if_missing = false;
  options.error_if_exists = false;

  rocksdb::DB *db_ptr = nullptr;

  std::vector<std::string> column_family_names;
  rocksdb::Status s = rocksdb::DB::ListColumnFamilies(options, data_path_,
                                                      &column_family_names);
  if (!s.ok()) {
    return RET_ERROR;
  }

  std::vector<rocksdb::ColumnFamilyDescriptor> column_families;
  for (const auto &name : column_family_names) {
    column_families.push_back(
        rocksdb::ColumnFamilyDescriptor(name, rocksdb::ColumnFamilyOptions()));
  }

  std::vector<rocksdb::ColumnFamilyHandle *> handles;
  rocksdb::Status status = rocksdb::DB::Open(
      options, data_path_, column_families, &handles, &db_ptr);
  if (!status.ok()) {
    return RET_ERROR;
  }

  data_.reset(db_ptr);
  for (size_t i = 0; i < column_family_names.size(); i++) {
    cf_handles_[column_family_names[i]] = handles[i];
  }

  return RET_OK;
}

RetNo Table::New() {
  Prepare();
  RetNo ret = NewData();
  return ret;
}

RetNo Table::Load() {
  assert(fs::exists(param_.path()));
  assert(fs::exists(data_path_));
  assert(fs::exists(index_path_));

  RetNo ret = RET_OK;
  ret = LoadData();
  if (ret != RET_OK) {
    return ret;
  }

  ret = LoadIndex();
  if (ret != RET_OK) {
    return ret;
  }

  return ret;
}

void Table::Prepare() {
  assert(!fs::exists(param_.path()));
  fs::create_directories(param_.path());
  fs::create_directories(data_path_);
  fs::create_directories(index_path_);

  PersistDescription();
}

RetNo Table::Add(int64_t id, std::vector<float> &vector,
                 const std::string &scalar, const WOptions &options,
                 bool normalize) {
  if (normalize) {
    Normalize(vector);
  }

  if (options.write_vector_to_data) {
    vdb::Id id_obj;
    id_obj.set_id(id);
    vdb::Vec vec_obj;
    vec_obj.mutable_data()->Assign(vector.begin(), vector.end());

    // 将ID和向量序列化
    std::string id_str;
    std::string vec_str;
    if (!id_obj.SerializeToString(&id_str) ||
        !vec_obj.SerializeToString(&vec_str)) {
      return RET_ERROR;
    }

    // 创建写批次以实现事务
    rocksdb::WriteBatch batch;

    // 将向量数据添加到批次中
    batch.Put(cf_handles_[kVectorColumnFamily], rocksdb::Slice(id_str),
              rocksdb::Slice(vec_str));

    // 如果有标量数据，也添加到批次中
    if (!scalar.empty()) {
      batch.Put(cf_handles_[kScalarColumnFamily], rocksdb::Slice(id_str),
                rocksdb::Slice(scalar));
    }

    // 作为一个事务提交批次
    rocksdb::WriteOptions write_options;
    rocksdb::Status status = data_->Write(write_options, &batch);
    if (!status.ok()) {
      return RET_ERROR;
    }
  }

  if (options.write_vector_to_index) {
    if (indexes_.empty()) {
      RetNo ret = BuildIndex();
      if (ret != RET_OK) {
        return ret;
      }
    }

    // 将向量添加到所有索引中
    for (auto &index_pair : indexes_) {
      RetNo ret = index_pair.second->Add(id, vector);
      if (ret != RET_OK) {
        return ret;
      }
    }
  }

  return RET_OK;
}

RetNo Table::Add(int64_t id, std::vector<float> &vector,
                 const WOptions &options, bool normalize) {
  return Add(id, vector, "", options, normalize);
}

RetNo Table::Get(int64_t id, std::vector<float> &vector, std::string &scalar) {
  // 创建ID对象并序列化
  vdb::Id id_obj;
  id_obj.set_id(id);
  std::string id_str;
  if (!id_obj.SerializeToString(&id_str)) {
    return RET_ERROR;
  }

  // 获取向量数据
  std::string vec_str;
  rocksdb::Status status =
      data_->Get(rocksdb::ReadOptions(), cf_handles_[kVectorColumnFamily],
                 rocksdb::Slice(id_str), &vec_str);

  if (!status.ok()) {
    if (status.IsNotFound()) {
      return RET_NOT_FOUND;
    }
    return RET_ERROR;
  }

  // 反序列化向量数据
  vdb::Vec vec_obj;
  if (!vec_obj.ParseFromString(vec_str)) {
    return RET_ERROR;
  }

  // 填充向量数据
  vector.clear();
  vector.assign(vec_obj.data().begin(), vec_obj.data().end());

  // 获取标量数据
  status = data_->Get(rocksdb::ReadOptions(), cf_handles_[kScalarColumnFamily],
                      rocksdb::Slice(id_str), &scalar);

  // 如果标量数据不存在，不视为错误，只返回空字符串
  if (status.IsNotFound()) {
    scalar.clear();
  } else if (!status.ok()) {
    return RET_ERROR;
  }

  return RET_OK;
}

RetNo Table::Get(int64_t id, std::vector<float> &vector) {
  std::string scalar;
  return Get(id, vector, scalar);
}

RetNo Table::Get(int64_t id, std::string &scalar) {
  // 创建ID对象并序列化
  vdb::Id id_obj;
  id_obj.set_id(id);
  std::string id_str;
  if (!id_obj.SerializeToString(&id_str)) {
    return RET_ERROR;
  }

  // 获取标量数据
  rocksdb::Status status =
      data_->Get(rocksdb::ReadOptions(), cf_handles_[kScalarColumnFamily],
                 rocksdb::Slice(id_str), &scalar);

  if (!status.ok()) {
    if (status.IsNotFound()) {
      return RET_NOT_FOUND;
    }
    return RET_ERROR;
  }

  return RET_OK;
}

RetNo Table::Search(const std::vector<float> &v, int32_t k,
                    std::vector<int64_t> &ids, std::vector<float> &distances,
                    std::vector<std::string> &scalars, const ROptions &options,
                    int32_t index_id) {
  if (index_id == -1) {
    index_id = MaxIndexID();
  }

  auto it = indexes_.find(index_id);
  if (it == indexes_.end()) {
    return RET_ERROR;
  }
  return DoSearch(it->second, v, k, ids, distances, scalars, options);
}

RetNo Table::Search(int64_t id, int32_t k, std::vector<int64_t> &ids,
                    std::vector<float> &distances,
                    std::vector<std::string> &scalars, const ROptions &options,
                    int32_t index_id) {
  if (index_id == -1) {
    index_id = MaxIndexID();
  }

  std::vector<float> v;
  RetNo ret = Get(id, v);
  if (ret != RET_OK) {
    return ret;
  }
  return Search(v, k, ids, distances, scalars, options, index_id);
}

// input: v, k
// output: ids, distances, scalars
RetNo Table::DoSearch(VIndexSPtr index, const std::vector<float> &v, int32_t k,
                      std::vector<int64_t> &ids, std::vector<float> &distances,
                      std::vector<std::string> &scalars,
                      const ROptions &options) {
  // 检查索引是否有效
  if (!index) {
    return RET_ERROR;
  }

  // 清空输出参数
  ids.clear();
  distances.clear();
  scalars.clear();

  // 使用索引执行向量搜索
  RetNo ret = index->Search(v, k, ids, distances);
  if (ret != RET_OK) {
    return ret;
  }

  // 如果没有找到结果，直接返回成功
  if (ids.empty()) {
    return RET_OK;
  }

  // 获取每个ID对应的标量数据
  for (const auto &id : ids) {
    // 创建ID对象并序列化
    vdb::Id id_obj;
    id_obj.set_id(id);
    std::string id_str;
    if (!id_obj.SerializeToString(&id_str)) {
      return RET_ERROR;
    }

    // 获取标量数据
    std::string scalar;
    rocksdb::Status status =
        data_->Get(rocksdb::ReadOptions(), cf_handles_[kScalarColumnFamily],
                   rocksdb::Slice(id_str), &scalar);

    // 如果标量数据不存在，添加空字符串
    if (status.IsNotFound()) {
      scalars.push_back("");
    } else if (!status.ok()) {
      return RET_ERROR;
    } else {
      scalars.push_back(scalar);
    }
  }

  return RET_OK;
}

RetNo Table::BuildIndex() {
  // 创建默认索引参数
  vdb::IndexParam param;

  // 设置索引ID为当前最大索引ID + 1
  int32_t index_id = MaxIndexID() + 1;
  param.set_path(index_path_ + "/" + std::to_string(index_id));
  param.set_id(index_id);
  param.set_create_time(TimeStamp().MilliSeconds());
  param.mutable_index_info()->CopyFrom(param_.default_index_info());

  // 调用带参数的BuildIndex函数
  return DoBuildIndex(param);
}

RetNo Table::BuildIndex(const vdb::FlatParam &param) {
  int32_t index_id = MaxIndexID() + 1;
  vdb::IndexParam param2;
  param2.set_path(index_path_ + "/" + std::to_string(index_id));
  param2.set_id(index_id);
  param2.set_create_time(TimeStamp().MilliSeconds());
  param2.mutable_index_info()->set_index_type(INDEX_TYPE_FLAT);
  param2.mutable_index_info()->mutable_flat_param()->CopyFrom(param);

  return DoBuildIndex(param2);
}

RetNo Table::BuildIndex(const vdb::HnswParam &param) {
  int32_t index_id = MaxIndexID() + 1;
  vdb::IndexParam param2;
  param2.set_path(index_path_ + "/" + std::to_string(index_id));
  param2.set_id(index_id);
  param2.set_create_time(TimeStamp().MilliSeconds());
  param2.mutable_index_info()->set_index_type(INDEX_TYPE_HNSW);
  param2.mutable_index_info()->mutable_hnsw_param()->CopyFrom(param);

  return DoBuildIndex(param2);
}

RetNo Table::DoBuildIndex(const vdb::IndexParam &param) {
  // 检查索引ID是否已存在
  if (indexes_.find(param.id()) != indexes_.end()) {
    return RET_ERROR;  // 索引ID已存在
  }

  // 检查索引目录是否存在
  if (fs::exists(param.path())) {
    return RET_ERROR;
  }

  // 创建索引实例
  VIndexSPtr index = std::make_shared<VIndex>(param);
  if (!index) {
    return RET_ERROR;
  }

  // 将索引添加到表的索引集合中
  indexes_[param.id()] = index;

  // 将索引添加到表参数中，以便持久化
  vdb::IndexParam *index_param = param_.add_indexes();
  *index_param = param;

  // 从数据库中读取所有向量并添加到索引中
  rocksdb::Iterator *it = data_->NewIterator(rocksdb::ReadOptions(),
                                             cf_handles_[kVectorColumnFamily]);
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    // 解析ID
    vdb::Id id_obj;
    if (!id_obj.ParseFromString(it->key().ToString())) {
      delete it;
      return RET_ERROR;
    }
    int64_t id = id_obj.id();

    // 解析向量
    vdb::Vec vec_obj;
    if (!vec_obj.ParseFromString(it->value().ToString())) {
      delete it;
      return RET_ERROR;
    }

    std::vector<float> vector(vec_obj.data().begin(), vec_obj.data().end());

    // 将向量添加到索引中
    RetNo ret = index->Add(id, vector);
    if (ret != RET_OK) {
      delete it;
      return ret;
    }
  }

  delete it;

  // 持久化表描述和索引
  PersistDescription();
  index->Persist();

  return RET_OK;
}

RetNo Table::DropIndex(int32_t left) {
  // 如果没有索引或者要保留的索引数大于等于当前索引数，则不需要删除
  if (indexes_.empty() || left >= static_cast<int32_t>(indexes_.size())) {
    return RET_OK;
  }

  // 如果要保留的索引数小于等于0，则删除所有索引
  if (left <= 0) {
    for (auto &index_pair : indexes_) {
      // 删除索引文件
      if (fs::exists(index_pair.second->param().path())) {
        fs::remove_all(index_pair.second->param().path());
      }
    }
    indexes_.clear();
    param_.clear_indexes();
    PersistDescription();
    return RET_OK;
  }

  // 按索引ID排序
  std::vector<int32_t> index_ids;
  for (const auto &index_pair : indexes_) {
    index_ids.push_back(index_pair.first);
  }
  std::sort(index_ids.begin(), index_ids.end());

  // 计算要删除的索引数量
  int32_t to_delete = index_ids.size() - left;
  if (to_delete <= 0) {
    return RET_OK;
  }

  // 删除旧的索引，保留最新的left个索引
  for (int32_t i = 0; i < to_delete; ++i) {
    int32_t index_id = index_ids[i];
    auto it = indexes_.find(index_id);
    if (it != indexes_.end()) {
      // 删除索引文件
      if (fs::exists(it->second->param().path())) {
        fs::remove_all(it->second->param().path());
      }
      indexes_.erase(it);
    }
  }

  // 更新表参数中的索引列表
  param_.clear_indexes();
  for (const auto &index_pair : indexes_) {
    vdb::IndexParam *index_param = param_.add_indexes();
    *index_param = index_pair.second->param();
  }

  // 持久化表描述
  PersistDescription();
  return RET_OK;
}

int32_t Table::MaxIndexID() const {
  int32_t max_id = -1;
  for (const auto &index : indexes_) {
    max_id = std::max(max_id, index.first);
  }
  return max_id;
}

RetNo Table::Persist() {
  PersistDescription();
  PersistIndex();
  return RET_OK;
}

void Table::PersistDescription() {
  std::ofstream file(description_file_);
  file << ToJson().dump(2);
  file.close();
}

json Table::ToJson() const {
  json j;
  j["param"] = param_.DebugString();
  j["persist_time"] = TimeStamp().ToString();
  return j;
}

void Table::PersistIndex() {
  for (const auto &index : indexes_) {
    index.second->Persist();
  }
}

std::vector<int32_t> Table::IndexIDs() const {
  std::vector<int32_t> ids;
  for (const auto &index : indexes_) {
    ids.push_back(index.first);
  }
  std::sort(ids.begin(), ids.end());
  return ids;
}

vdb::FlatParam DefaultFlatParam(int32_t dim) {
  vdb::FlatParam param;
  param.set_dim(dim);
  param.set_max_elements(1000000);
  param.set_distance_type(DISTANCE_TYPE_INNER_PRODUCT);
  return param;
}

vdb::HnswParam DefaultHnswParam(int32_t dim) {
  vdb::HnswParam param;
  param.set_dim(dim);
  param.set_max_elements(1000000);
  param.set_m(16);
  param.set_ef_construction(100);
  param.set_distance_type(DISTANCE_TYPE_INNER_PRODUCT);
  return param;
}

}  // namespace vectordb