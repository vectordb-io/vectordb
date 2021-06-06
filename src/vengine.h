#ifndef __VECTORDB_VENGINE_H__
#define __VECTORDB_VENGINE_H__

#include <vector>
#include <string>
#include "vec.h"
#include "slice.h"
#include "options.h"
#include "status.h"

namespace vectordb {

class VEngine {
  public:
    VEngine() = default;
    VEngine(const VEngine&) = delete;
    VEngine& operator=(const VEngine&) = delete;
    virtual ~VEngine() {};

    virtual Status Open(const Options &options, const std::string &name, VEngine** vengine) = 0;
    virtual Status Put(const WriteOptions &options, const std::string &key, const Vec &result) = 0;
    virtual Status Get(const ReadOptions &options, const std::string &key, Vec &result) = 0;
    virtual Status Delete(const WriteOptions &options, const std::string &key) = 0;
    virtual Status GetKNN(const ReadOptions &options, const std::string &key, std::vector<Vec> &results) = 0;
    virtual Status GetKNN(const ReadOptions &options, const Vec &vec, std::vector<Vec> &results) = 0;
    virtual Status BuildIndex() = 0;
    virtual bool HasIndex() = 0;

};

}  // namespace vectordb

#endif
