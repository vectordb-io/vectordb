#ifndef VECTORDB_ENGINE_META_H_
#define VECTORDB_ENGINE_META_H_

#include <memory>

#include "leveldb/db.h"
#include "nlohmann/json.hpp"

namespace vectordb {

#define ENGINE_META_KEY_DIM "dim"

class EngineMeta final {
 public:
  explicit EngineMeta(const std::string &path);
  ~EngineMeta();
  EngineMeta(const EngineMeta &) = delete;
  EngineMeta &operator=(const EngineMeta &) = delete;

  int32_t dim() const;
  void SetDim(int32_t dim);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

 private:
  void Init();
  void PersistDim();

 private:
  std::string path_;
  leveldb::Options db_options_;
  std::shared_ptr<leveldb::DB> db_;

  int32_t dim_;
};

inline EngineMeta::~EngineMeta() {}

inline int32_t EngineMeta::dim() const { return dim_; }

}  // namespace vectordb

#endif
