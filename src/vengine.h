#ifndef __VECTORDB_VENGINE_H__
#define __VECTORDB_VENGINE_H__

#include <map>
#include <vector>
#include <string>
#include <leveldb/db.h>
#include "vec.h"
#include "slice.h"
#include "options.h"
#include "status.h"
#include "vindex.h"

namespace vectordb {

class VEngine {
  public:
    VEngine() = default;
    VEngine(const VEngine&) = delete;
    VEngine& operator=(const VEngine&) = delete;
    virtual ~VEngine() {};

    Status Open(const Options &options, const std::string &name, VEngine** vengine);
    Status Put(const WriteOptions &options, const std::string &key, const Vec &result);
    Status Get(const ReadOptions &options, const std::string &key, Vec &result);
    Status Delete(const WriteOptions &options, const std::string &key);
    Status GetKNN(const ReadOptions &options, const std::string &key, std::vector<VecDt> &results);
    Status GetKNN(const ReadOptions &options, const Vec &vec, std::vector<VecDt> &results);
    Status BuildIndex();
    bool HasIndex();

  private:
    std::string path_;
    leveldb::DB* data_;

    std::map<std::string, VIndex*> indices_;
};

} // namespace vectordb

#endif
