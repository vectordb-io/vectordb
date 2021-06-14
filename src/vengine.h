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
    VEngine(std::string path, int dim, const std::map<std::string, std::string> &indices);
    VEngine(const VEngine&) = delete;
    VEngine& operator=(const VEngine&) = delete;
    ~VEngine();

    Status Put(const std::string &key, const VecObj &vo);
    Status Get(const std::string &key, VecObj &vo) const;
    Status Delete(const std::string &key);

    bool HasIndex() const;
    Status AddIndex(std::string index_name, std::string index_type);
    Status GetKNN(const std::string &key, std::vector<VecDt> &results, const std::string &index_type);
    Status GetKNN(const Vec &vec, std::vector<VecDt> &results, const std::string &index_type);

    Status Init();

    leveldb::DB* data() {
        return data_;
    }

    int dim() const {
        return dim_;
    }

  private:
    Status Build();
    Status BuildData();
    Status Load();
    Status BuildIndex();
    Status LoadData();
    Status LoadIndex();

    std::string path_;
    std::string data_path_;
    std::string index_path_;

    int dim_;
    leveldb::DB* data_;
    //std::map<std::string, VIndex*> indices_;
    std::map<std::string, std::shared_ptr<VIndex>> indices_;
    std::map<std::string, std::string> indices_name_type_;
};

} // namespace vectordb

#endif
