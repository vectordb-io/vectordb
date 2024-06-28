#ifndef VECTORDB_VENGINE_H_
#define VECTORDB_VENGINE_H_

#include <memory>

#include "engine_meta.h"
#include "leveldb/db.h"
#include "nlohmann/json.hpp"
#include "vdb_common.h"
#include "vindex.h"

namespace vectordb {

struct AddIndexParam {
  uint64_t timestamp;
  int32_t dim;
  VIndexType index_type;       // uint8_t
  DistanceType distance_type;  // uint8_t
  int32_t annoy_tree_num;
};

struct Vec {
  std::vector<float> data;
  int32_t dim() const { return static_cast<int32_t>(data.size()); }

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

struct VecValue {
  Vec vec;
  std::string attach_value;

  int32_t dim() const { return vec.dim(); }

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

struct VecObj {
  std::string key;
  VecValue vv;

  int32_t dim() const { return vv.dim(); }

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

class VEngine final : public std::enable_shared_from_this<VEngine> {
 public:
  explicit VEngine(const std::string &path, int32_t dim);
  ~VEngine();
  VEngine(const VEngine &) = delete;
  VEngine &operator=(const VEngine &) = delete;

  std::string path() const;
  std::string meta_path() const;
  std::string data_path() const;
  std::string index_path() const;
  std::shared_ptr<leveldb::DB> db();
  int32_t Dim() const;

  int32_t Put(const std::string &key, VecValue &vv);
  int32_t Get(const std::string &key, VecObj &vo) const;
  int32_t Delete(const std::string &key);
  int32_t Load(const std::string &file_path);

  bool HasIndex() const;
  int32_t AddIndex(AddIndexParam param);
  int32_t GetKNN(const std::string &key, std::vector<VecResult> &results,
                 int limit);
  int32_t GetKNN(const std::vector<float> &vec, std::vector<VecResult> &results,
                 int limit);
  void LoadIndex();

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

 private:
  void Init();
  void MkDir();

 private:
  std::string path_;
  std::string meta_path_;
  std::string data_path_;
  std::string index_path_;

  leveldb::Options db_options_;
  std::shared_ptr<leveldb::DB> db_;
  EngineMetaSPtr meta_;
  VindexManagerSPtr index_manager_;
};

inline VEngine::~VEngine() {}

inline std::string VEngine::path() const { return path_; }

inline std::string VEngine::meta_path() const { return meta_path_; }

inline std::string VEngine::data_path() const { return data_path_; }

inline std::string VEngine::index_path() const { return index_path_; }

}  // namespace vectordb

#endif
