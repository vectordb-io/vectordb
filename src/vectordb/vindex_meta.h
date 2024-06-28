#ifndef VECTORDB_VINDEX_META_H_
#define VECTORDB_VINDEX_META_H_

#include <memory>

#include "leveldb/db.h"
#include "nlohmann/json.hpp"
#include "vindex.h"

namespace vectordb {

#define VINDEX_META_KEY "0"

class VindexMeta final {
 public:
  explicit VindexMeta(const std::string &path, VIndexParam &param);
  ~VindexMeta();
  VindexMeta(const VindexMeta &) = delete;
  VindexMeta &operator=(const VindexMeta &) = delete;

  VIndexParam param() const;
  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

 private:
  void Init();
  void Persist();
  int32_t CreateDB();

 private:
  const std::string path_;
  leveldb::Options db_options_;
  std::shared_ptr<leveldb::DB> db_;

  VIndexParam param_;
};

inline VindexMeta::~VindexMeta() {}

inline VIndexParam VindexMeta::param() const { return param_; }

}  // namespace vectordb

#endif
