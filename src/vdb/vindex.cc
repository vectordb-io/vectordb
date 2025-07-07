#include "vindex.h"

#include <fstream>

#include "pb2json.h"
#include "util.h"

namespace vectordb {

VIndex::VIndex(const vdb::IndexParam &param)
    : data_path_(param.path() + "/data"),
      description_file_(param.path() + "/description.json"),
      param_(param),
      hindex_file_(data_path_ + "/index.bin") {
  Init();
}

VIndex::~VIndex() { Persist(); }

void VIndex::Init() {
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

RetNo VIndex::New() {
  Prepare();
  RetNo ret = NewIndex();
  return ret;
}

RetNo VIndex::Load() {
  assert(fs::exists(param_.path()));
  assert(fs::exists(data_path_));
  assert(fs::exists(description_file_));

  RetNo ret = LoadIndex();
  return ret;
}

void VIndex::Prepare() {
  fs::create_directories(param_.path());
  fs::create_directories(data_path_);

  PersistDescription();
}

RetNo VIndex::Add(int64_t id, const std::vector<float> &vector) {
  switch (param_.index_info().index_type()) {
    case INDEX_TYPE_FLAT:
    case INDEX_TYPE_HNSW: {
      assert(hindex_);
      hindex_->addPoint(vector.data(), id);
      return RET_OK;
    }

    default: {
      return RET_ERROR;
    }
  }
  return RET_ERROR;
}

RetNo VIndex::Search(const std::vector<float> &vector, int32_t k,
                     std::vector<int64_t> &ids, std::vector<float> &distances) {
  // 检查向量维度
  int32_t dim = 0;
  switch (param_.index_info().index_type()) {
    case INDEX_TYPE_FLAT: {
      dim = param_.index_info().flat_param().dim();
      break;
    }

    case INDEX_TYPE_HNSW: {
      dim = param_.index_info().hnsw_param().dim();
      break;
    }

    default: {
      return RET_ERROR;
    }
  }

  if (vector.size() != static_cast<size_t>(dim)) {
    return RET_ERROR;
  }

  int32_t actual_k = std::min(k, Size());
  std::priority_queue<std::pair<float, size_t>> results;

  switch (param_.index_info().index_type()) {
    case INDEX_TYPE_FLAT:
    case INDEX_TYPE_HNSW: {
      assert(hindex_);
      results = hindex_->searchKnn(vector.data(), actual_k);
      break;
    }

    default: {
      return RET_ERROR;
    }
  }

  {
    // 清空输出参数
    ids.clear();
    distances.clear();

    // 预留空间
    ids.reserve(results.size());
    distances.reserve(results.size());

    // 将结果填充到输出参数中
    while (!results.empty()) {
      ids.push_back(results.top().second);
      distances.push_back(results.top().first);
      results.pop();
    }

    // 由于优先队列是最大堆，结果需要反转以保持距离升序
    std::reverse(ids.begin(), ids.end());
    std::reverse(distances.begin(), distances.end());
  }

  return RET_OK;
}

RetNo VIndex::GetVecByID(int64_t id, std::vector<float> &v) {
  v.clear();
  switch (param_.index_info().index_type()) {
    case INDEX_TYPE_FLAT: {
      hnswlib::BruteforceSearch<float> *flat_index =
          static_cast<hnswlib::BruteforceSearch<float> *>(hindex_.get());

      // 获取内部索引
      auto search = flat_index->dict_external_to_internal.find(id);
      if (search == flat_index->dict_external_to_internal.end()) {
        return RET_ERROR;
      }

      size_t internal_idx = search->second;
      if (internal_idx >= flat_index->cur_element_count) {
        return RET_ERROR;
      }

      // 获取向量数据
      char *data_ptr =
          flat_index->data_ + flat_index->size_per_element_ * internal_idx;
      size_t dim = flat_index->data_size_ / sizeof(float);
      assert(dim ==
             static_cast<size_t>(param_.index_info().flat_param().dim()));

      v.resize(dim);
      memcpy(v.data(), data_ptr, dim * sizeof(float));
      break;
    }

    case INDEX_TYPE_HNSW: {
      hnswlib::HierarchicalNSW<float> *hnsw_index =
          static_cast<hnswlib::HierarchicalNSW<float> *>(hindex_.get());

      // 获取内部索引
      auto search = hnsw_index->label_lookup_.find(id);
      if (search == hnsw_index->label_lookup_.end()) {
        return RET_ERROR;
      }

      size_t internal_idx = search->second;
      if (internal_idx >= hnsw_index->getCurrentElementCount()) {
        return RET_ERROR;
      }

      // 获取向量数据
      size_t dim = param_.index_info().hnsw_param().dim();

      v.resize(dim);
      memcpy(v.data(), hnsw_index->getDataByInternalId(internal_idx),
             dim * sizeof(float));
      break;
    }

    default: {
      return RET_ERROR;
    }
  }

  return RET_OK;
}

RetNo VIndex::Search(int64_t id, int32_t k, std::vector<int64_t> &ids,
                     std::vector<float> &distances) {
  std::vector<float> v;
  RetNo ret = GetVecByID(id, v);
  if (ret != RET_OK) {
    return ret;
  }

  ret = Search(v, k, ids, distances);
  return ret;
}

RetNo VIndex::Persist() {
  PersistDescription();
  PersistIndex();
  return RET_OK;
}

json VIndex::ToJson() const {
  json j;
  j["param"] = IndexParamToJson(param_);
  j["persist_time"] = TimeStamp().ToString();
  return j;
}

void VIndex::PersistDescription() {
  std::ofstream file(description_file_);
  file << ToJson().dump(2);
  file.close();
}

void VIndex::PersistIndex() {
  if (hindex_) {
    hindex_->saveIndex(hindex_file_);
  }
}

int32_t VIndex::Size() const {
  switch (param_.index_info().index_type()) {
    case INDEX_TYPE_FLAT: {
      hnswlib::BruteforceSearch<float> *flat_index =
          static_cast<hnswlib::BruteforceSearch<float> *>(hindex_.get());
      return flat_index->cur_element_count;
    }

    case INDEX_TYPE_HNSW: {
      hnswlib::HierarchicalNSW<float> *hnsw_index =
          static_cast<hnswlib::HierarchicalNSW<float> *>(hindex_.get());
      return hnsw_index->getCurrentElementCount();
    }

    default: {
      return 0;
    }
  }
}

RetNo VIndex::NewIndex() {
  switch (param_.index_info().index_type()) {
    case INDEX_TYPE_FLAT: {
      assert(param_.index_info().has_flat_param());

      if (param_.index_info().flat_param().distance_type() ==
          DISTANCE_TYPE_L2) {
        hspace_ = std::make_shared<hnswlib::L2Space>(
            param_.index_info().flat_param().dim());
        hindex_ = std::make_unique<hnswlib::BruteforceSearch<float>>(
            hspace_.get(), param_.index_info().flat_param().max_elements());
        assert(hindex_);

      } else if (param_.index_info().flat_param().distance_type() ==
                 DISTANCE_TYPE_INNER_PRODUCT) {
        hspace_ = std::make_shared<hnswlib::InnerProductSpace>(
            param_.index_info().flat_param().dim());
        hindex_ = std::make_unique<hnswlib::BruteforceSearch<float>>(
            hspace_.get(), param_.index_info().flat_param().max_elements());
        assert(hindex_);

      } else {
        return RET_ERROR;
      }

      break;
    }

    case INDEX_TYPE_HNSW: {
      assert(param_.index_info().has_hnsw_param());

      if (param_.index_info().hnsw_param().distance_type() ==
          DISTANCE_TYPE_L2) {
        hspace_ = std::make_shared<hnswlib::L2Space>(
            param_.index_info().hnsw_param().dim());
        hindex_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(
            hspace_.get(), param_.index_info().hnsw_param().max_elements(),
            param_.index_info().hnsw_param().m(),
            param_.index_info().hnsw_param().ef_construction());
        assert(hindex_);

      } else if (param_.index_info().hnsw_param().distance_type() ==
                 DISTANCE_TYPE_INNER_PRODUCT) {
        hspace_ = std::make_shared<hnswlib::InnerProductSpace>(
            param_.index_info().hnsw_param().dim());
        hindex_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(
            hspace_.get(), param_.index_info().hnsw_param().max_elements(),
            param_.index_info().hnsw_param().m(),
            param_.index_info().hnsw_param().ef_construction());
        assert(hindex_);

      } else {
        return RET_ERROR;
      }

      break;
    }

    default: {
      return RET_ERROR;
    }
  }

  return RET_OK;
}

RetNo VIndex::LoadIndex() {
  switch (param_.index_info().index_type()) {
    case INDEX_TYPE_FLAT: {
      assert(fs::exists(hindex_file_));
      assert(param_.index_info().has_flat_param());

      if (param_.index_info().flat_param().distance_type() ==
          DISTANCE_TYPE_L2) {
        hspace_ = std::make_shared<hnswlib::L2Space>(
            param_.index_info().flat_param().dim());
        hindex_ = std::make_unique<hnswlib::BruteforceSearch<float>>(
            hspace_.get(), hindex_file_);
        assert(hindex_);

      } else if (param_.index_info().flat_param().distance_type() ==
                 DISTANCE_TYPE_INNER_PRODUCT) {
        hspace_ = std::make_shared<hnswlib::InnerProductSpace>(
            param_.index_info().flat_param().dim());
        hindex_ = std::make_unique<hnswlib::BruteforceSearch<float>>(
            hspace_.get(), hindex_file_);
        assert(hindex_);

      } else {
        return RET_ERROR;
      }

      break;
    }

    case INDEX_TYPE_HNSW: {
      assert(fs::exists(hindex_file_));
      assert(param_.index_info().has_hnsw_param());

      if (param_.index_info().hnsw_param().distance_type() ==
          DISTANCE_TYPE_INNER_PRODUCT) {
        hspace_ = std::make_shared<hnswlib::L2Space>(
            param_.index_info().hnsw_param().dim());
        hindex_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(
            hspace_.get(), hindex_file_);
        assert(hindex_);

      } else if (param_.index_info().hnsw_param().distance_type() ==
                 DISTANCE_TYPE_INNER_PRODUCT) {
        hspace_ = std::make_shared<hnswlib::InnerProductSpace>(
            param_.index_info().hnsw_param().dim());
        hindex_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(
            hspace_.get(), hindex_file_);
        assert(hindex_);

      } else {
        return RET_ERROR;
      }

      break;
    }

    default: {
      return RET_ERROR;
    }
  }

  return RET_OK;
}

}  // namespace vectordb