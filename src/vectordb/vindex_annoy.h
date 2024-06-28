#ifndef VECTORDB_VINDEX_ANNOY_H_
#define VECTORDB_VINDEX_ANNOY_H_

#include <memory>

#include "annoylib.h"
#include "engine_meta.h"
#include "kissrandom.h"
#include "leveldb/db.h"
#include "nlohmann/json.hpp"
#include "vdb_common.h"
#include "vindex.h"

namespace vectordb {

using Annoy = AnnoyIndexInterface<int32_t, float>;
using AnnoySPtr = std::shared_ptr<Annoy>;
using AnnoyUPtr = std::unique_ptr<Annoy>;
using AnnoyWPtr = std::weak_ptr<Annoy>;

AnnoySPtr CreateAnnoy(const VIndexParam &param);

class VindexAnnoy : public Vindex {
 public:
  explicit VindexAnnoy(VIndexParam &param, VEngineSPtr v);
  ~VindexAnnoy();
  VindexAnnoy(const VindexAnnoy &) = delete;
  VindexAnnoy &operator=(const VindexAnnoy &) = delete;

  int32_t GetKNN(const std::string &key, std::vector<VecResult> &results,
                 int limit = DEFAULT_LIMIT) override;
  int32_t GetKNN(const std::vector<float> &vec, std::vector<VecResult> &results,
                 int limit = DEFAULT_LIMIT) override;

  nlohmann::json ToJson() override;
  nlohmann::json ToJsonTiny() override;
  std::string ToJsonString(bool tiny, bool one_line) override;

 private:
  void Init();
  void MkDir();
  int32_t Build();
  int32_t Load();
  int32_t PrepareResults(const std::vector<int32_t> results,
                         const std::vector<float> distances,
                         std::vector<VecResult> &results_out);

 private:
  std::string keyid_path_;
  std::string meta_path_;
  std::string annoy_path_file_;

  KeyidMetaSPtr keyid_;
  VindexMetaSPtr meta_;
  AnnoySPtr annoy_index_;
  VEngineWPtr vengine_;
};

inline VindexAnnoy::~VindexAnnoy() {}

}  // namespace vectordb

#endif
