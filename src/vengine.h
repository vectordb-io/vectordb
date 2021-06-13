#ifndef __VECTORDB_VENGINE_H__
#define __VECTORDB_VENGINE_H__

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <leveldb/db.h>
#include "vec.h"
#include "slice.h"
#include "options.h"
#include "status.h"
#include "vindex.h"

namespace vectordb {

class VEngine {
  public:
    VEngine(std::string path, const std::map<std::string, std::string> &indices);
    VEngine(const VEngine&) = delete;
    VEngine& operator=(const VEngine&) = delete;
    ~VEngine();

    Status Put(const std::string &key, const Vec &v);
    Status Get(const std::string &key, Vec &v);
    Status Delete(const std::string &key);

    bool HasIndex() const;
    Status BuildIndex(std::string index_name, std::string index_type);
    Status Distance(const Vec &v1, const Vec &v2, double &result) const;
    Status Distance(const std::string &key1, const std::string &key2, double &result) const;
    Status GetKNN(const std::string &key, std::vector<VecDt> &results, const std::string &index_type);
    Status GetKNN(const Vec &vec, std::vector<VecDt> &results, const std::string &index_type);

  private:
    Status Mkdir();
    Status InitData();
    Status InitIndices(const std::map<std::string, std::string> &indices);

    std::string path_;
    std::string data_path_;
    std::string index_path_;

    leveldb::DB* data_;
    std::map<std::string, std::shared_ptr<VIndex>> indices_;
};

} // namespace vectordb

#endif
