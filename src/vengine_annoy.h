#ifndef __VECTORDB_VENGINE_ANNOY_H__
#define __VECTORDB_VENGINE_ANNOY_H__

#include <leveldb/db.h>
#include "vengine.h"

namespace vectordb {

class VEngineAnnoy : public VEngine {
  public:
    VEngineAnnoy();
    VEngineAnnoy(const VEngineAnnoy&) = delete;
    VEngineAnnoy& operator=(const VEngineAnnoy&) = delete;
    ~VEngineAnnoy();

    virtual Status Open(const Options &options, const std::string &name, VEngine** vengine) override;
    virtual Status Put(const WriteOptions &options, const std::string &key, const Vec &result) override;
    virtual Status Get(const ReadOptions &options, const std::string &key, Vec &result) override;
    virtual Status Delete(const WriteOptions &options, const std::string &key) override;
    virtual Status GetKNN(const ReadOptions &options, const std::string &key, std::vector<Vec> &results) override;
    virtual Status GetKNN(const ReadOptions &options, const Vec &vec, std::vector<Vec> &results) override;
    virtual Status BuildIndex() override;
    virtual bool HasIndex() override;

  private:
    std::string path_;
    leveldb::DB* db_;
};


}  // namespace vectordb

#endif
