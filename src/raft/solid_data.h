#ifndef VRAFT_SOLID_DATA_H_
#define VRAFT_SOLID_DATA_H_

#include <stdint.h>

#include <cassert>
#include <memory>

#include "leveldb/db.h"
#include "nlohmann/json.hpp"

namespace vraft {

const uint8_t kTermKey = 0;
const uint8_t kVoteKey = 1;

class SolidData final {
 public:
  SolidData(const std::string &path);
  ~SolidData();
  SolidData(const SolidData &t) = delete;
  SolidData &operator=(const SolidData &t) = delete;
  void Init();

  std::string path() const { return path_; }

  uint64_t term() const { return term_; }
  uint64_t vote() const { return vote_; }

  void IncrTerm();
  void SetTerm(uint64_t term);
  void SetVote(uint64_t vote);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

 private:
  void PersistTerm();
  void PersistVote();

 private:
  uint64_t term_;
  uint64_t vote_;

  std::string path_;
  leveldb::Options db_options_;
  std::shared_ptr<leveldb::DB> db_;
};

}  // namespace vraft

#endif
