#include "vindex_annoy.h"

#include "annoylib.h"
#include "common.h"
#include "keyid_meta.h"
#include "kissrandom.h"
#include "util.h"
#include "vengine.h"
#include "vindex_meta.h"
#include "vraft_logger.h"

namespace vectordb {

AnnoySPtr CreateAnnoy(const VIndexParam &param) {
  AnnoySPtr sptr = nullptr;
  Annoy *ptr = nullptr;
  switch (param.distance_type) {
    case kCosine: {
      ptr = new AnnoyIndex<int32_t, float, Angular, Kiss32Random,
                           AnnoyIndexMultiThreadedBuildPolicy>(param.dim);
      sptr.reset(ptr);
      return sptr;
    }

    case kInnerProduct: {
      ptr = new AnnoyIndex<int32_t, float, DotProduct, Kiss32Random,
                           AnnoyIndexMultiThreadedBuildPolicy>(param.dim);
      sptr.reset(ptr);
      return sptr;
    }

    case kEuclidean: {
      ptr = new AnnoyIndex<int32_t, float, Euclidean, Kiss32Random,
                           AnnoyIndexMultiThreadedBuildPolicy>(param.dim);
      sptr.reset(ptr);
      return sptr;
    }

    default:
      return sptr;
  }
}

VindexAnnoy::VindexAnnoy(VIndexParam &param, VEngineSPtr v)
    : Vindex(param),
      keyid_path_(param.path + "/keyid"),
      meta_path_(param.path + "/meta"),
      annoy_path_file_(param.path + "/annoy.tree"),
      vengine_(v) {
  Init();
}

int32_t VindexAnnoy::GetKNN(const std::string &key,
                            std::vector<VecResult> &results, int limit) {
  int32_t id, rv;

  if (limit > 0) {
    rv = keyid_->Get(key, id);
    if (rv != 0) {
      return -1;
    }

    std::vector<int> annoy_result;
    std::vector<float> distances;

    annoy_index_->get_nns_by_item(id, limit, SEARCH_K, &annoy_result,
                                  &distances);
    assert(annoy_result.size() == distances.size());

    rv = PrepareResults(annoy_result, distances, results);
    assert(rv == 0);
  }

  return 0;
}

int32_t VindexAnnoy::GetKNN(const std::vector<float> &vec,
                            std::vector<VecResult> &results, int limit) {
  if (static_cast<int32_t>(vec.size()) != param().dim) {
    vraft::vraft_logger.FError("build index dim error, %u != %d", vec.size(),
                               param().dim);
    return -1;
  }

  if (limit > 0) {
    std::vector<int32_t> annoy_result;
    std::vector<float> distances;
    annoy_index_->get_nns_by_vector(vec.data(), limit, SEARCH_K, &annoy_result,
                                    &distances);
    assert(annoy_result.size() == distances.size());

    int32_t rv = PrepareResults(annoy_result, distances, results);
    assert(rv == 0);
  }

  return 0;
}

nlohmann::json VindexAnnoy::ToJson() {
  nlohmann::json j;
  j["keyid_path"] = keyid_path_;
  j["annoy_path_file"] = annoy_path_file_;
  j["meta_path_"] = meta_path_;
  j["meta"] = meta_->ToJson();
  return j;
}

nlohmann::json VindexAnnoy::ToJsonTiny() { return ToJson(); }

std::string VindexAnnoy::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["vindex-annoy"] = ToJsonTiny();
  } else {
    j["vindex-annoy"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

void VindexAnnoy::Init() {
  bool dir_exist = vraft::IsDirExist(param().path);
  if (!dir_exist) {
    MkDir();
  }

  // init param meta, dir logic in VindexMeta
  VIndexParam tmp_param = param();
  meta_ = std::make_shared<VindexMeta>(meta_path_, tmp_param);
  assert(meta_);

  // update parent param
  set_param(meta_->param());

  // init keyid meta, dir logic in KeyidMeta
  keyid_ = std::make_shared<KeyidMeta>(keyid_path_);
  assert(keyid_);

  // init annoy index
  annoy_index_ = CreateAnnoy(param());
  assert(annoy_index_);

  bool annoy_file_exist = vraft::IsFileExist(annoy_path_file_);
  if (!annoy_file_exist) {
    auto rv = Build();  // file not exist, build
    assert(rv == 0);

  } else {
    auto rv = Load();  // file exist, load
    assert(rv == 0);
  }
}

void VindexAnnoy::MkDir() {
  char cmd[128];
  snprintf(cmd, sizeof(cmd), "mkdir -p %s", param().path.c_str());
  system(cmd);
}

int32_t VindexAnnoy::Build() {
  assert(annoy_index_);
  VEngineSPtr ve = vengine_.lock();
  if (ve) {
    int32_t annoy_index_id = 0;
    leveldb::Iterator *it = ve->db()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      std::string key = it->key().ToString();
      std::string value = it->value().ToString();

      VecValue vv;
      int32_t bytes = vv.FromString(value);
      assert(bytes > 0);

      if (vv.dim() != param().dim) {
        vraft::vraft_logger.FError("build index dim error, %d != %d", vv.dim(),
                                   param().dim);
        return -1;
      }

      std::vector<float> arr;
      for (int j = 0; j < vv.dim(); ++j) {
        arr.push_back(vv.vec.data[j]);
      }
      const float *parr = &(*arr.begin());
      annoy_index_->add_item(annoy_index_id, parr);

      int32_t rv = keyid_->Put(key, annoy_index_id);
      assert(rv == 0);

      rv = keyid_->Put(annoy_index_id, key);
      assert(rv == 0);

      ++annoy_index_id;
    }

    if (!it->status().ok()) {
      vraft::vraft_logger.FError("build index %s",
                                 it->status().ToString().c_str());
      return -1;
    }
    delete it;

    annoy_index_->build(param().annoy_tree_num);
    vraft::vraft_logger.FInfo("annoy build tree finish, path:%s", param().path);

    annoy_index_->save(annoy_path_file_.c_str());
    vraft::vraft_logger.FInfo("annoy save tree finish, path:%s", param().path);

    return 0;

  } else {
    return -1;
  }
}

int32_t VindexAnnoy::Load() {
  assert(annoy_index_);
  auto b = annoy_index_->load(annoy_path_file_.c_str());
  assert(b);
  return 0;
}

int32_t VindexAnnoy::PrepareResults(const std::vector<int32_t> results,
                                    const std::vector<float> distances,
                                    std::vector<VecResult> &results_out) {
  assert(results.size() == distances.size());
  results_out.clear();

  VEngineSPtr ve = vengine_.lock();
  if (!ve) {
    return -1;
  }

  for (size_t i = 0; i < results.size(); ++i) {
    std::string find_key;

    int32_t rv = keyid_->Get(results[i], find_key);
    assert(rv == 0);

    VecObj vo;
    rv = ve->Get(find_key, vo);
    assert(rv == 0);
    assert(find_key == vo.key);

    VecResult vr;
    vr.key = vo.key;
    vr.attach_value = vo.vv.attach_value;
    vr.distance = distances[i];

    results_out.push_back(vr);
  }

  std::sort(results_out.begin(), results_out.end(), std::less<VecResult>());
  return 0;
}

}  // namespace vectordb
