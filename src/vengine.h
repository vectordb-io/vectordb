#ifndef __VECTORDB_VENGINE_H__
#define __VECTORDB_VENGINE_H__

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include "jsonxx/json.hpp"
#include <leveldb/db.h>
#include "vec.h"
#include "slice.h"
#include "options.h"
#include "status.h"
#include "vindex.h"
#include "vindex_manager.h"

namespace vectordb {

struct VEngineParam {
    int dim;
    std::string replica_name;
};

class VEngine {
  public:
#define KEY_VENGINE_META_PERSIST "KEY_VENGINE_META_PERSIST"

  public:
    VEngine(std::string path, const VEngineParam& param);  // call Init
    VEngine(std::string path);                             // call Load
    VEngine(const VEngine&) = delete;
    VEngine& operator=(const VEngine&) = delete;
    ~VEngine();

    Status Init();
    Status Load();
    Status Put(const std::string &key, const VecObj &vo);
    Status Get(const std::string &key, VecObj &vo) const;
    Status Delete(const std::string &key);

    bool HasIndex() const;
    Status AddIndex(std::string index_type, void *param);
    Status GetKNN(const std::string &key, int limit, std::vector<VecDt> &results, const std::string &index_name);
    Status GetKNN(const std::vector<float> &vec, int limit, std::vector<VecDt> &results, const std::string &index_name);

    // append into keys
    //Status Keys(std::vector<std::string> &keys) const;

    leveldb::DB* mutable_db_data() {
        return db_data_;
    }

    leveldb::DB* mutable_db_meta() {
        return db_meta_;
    }

    VIndexManager& mutable_vindex_manager() {
        return vindex_manager_;
    }

    int dim() const {
        return dim_;
    }

    const std::string& replica_name() const {
        return replica_name_;
    }

    jsonxx::json64 ToJson() const;
    std::string ToString() const;
    std::string ToStringPretty() const;

  private:
    Status PersistMeta();
    Status LoadMeta();
    Status LoadData();
    Status LoadIndex();

    std::string path_;
    std::string data_path_;
    std::string meta_path_;
    std::string index_path_;

    int dim_;
    std::string replica_name_;
    leveldb::DB* db_data_;
    leveldb::DB* db_meta_;

    VIndexManager vindex_manager_;
};

} // namespace vectordb

#endif
