#ifndef VECTORDB_KEYID_META_H_
#define VECTORDB_KEYID_META_H_

#include <memory>
#include <string>

#include "leveldb/db.h"
#include "nlohmann/json.hpp"
#include "slice.h"

namespace vectordb {

#define ANNOY_KEY_PREFIX "key_"
std::string EncodeAnnoyKey(const std::string &key);
vraft::Slice DecodeAnnoyKey(const std::string &key);

class KeyidMeta final {
 public:
  explicit KeyidMeta(const std::string &path);
  ~KeyidMeta();
  KeyidMeta(const KeyidMeta &) = delete;
  KeyidMeta &operator=(const KeyidMeta &) = delete;

  int32_t Put(const std::string &vkey, int32_t id);
  int32_t Put(int32_t id, const std::string &vkey);
  int32_t Get(const std::string &vkey, int32_t &id);
  int32_t Get(int32_t id, std::string &vkey);

 private:
  int32_t CreateDB();

 private:
  std::string path_;
  leveldb::Options db_options_;
  std::shared_ptr<leveldb::DB> db_;
};

inline KeyidMeta::~KeyidMeta() {}

}  // namespace vectordb

#endif
