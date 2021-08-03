#ifndef __VECTORDB_VINDEX_KNN_GRAPH_H__
#define __VECTORDB_VINDEX_KNN_GRAPH_H__

#include <string>
#include "leveldb/db.h"
#include "vec.h"
#include "status.h"
#include "vindex.h"

namespace vectordb {

struct KNNGraphParam {
    int k;
    std::vector<std::string>* all_keys;
};

class VIndexKNNGraph : public VIndex {
  public:
    VIndexKNNGraph(const std::string &path, VEngine* vengine, KNNGraphParam *param);  // call Build
    VIndexKNNGraph(const std::string &path, VEngine* vengine);                        // call Load
    VIndexKNNGraph(const VIndexKNNGraph&) = delete;
    VIndexKNNGraph& operator=(const VIndexKNNGraph&) = delete;
    ~VIndexKNNGraph();

    Status GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) override;
    Status GetKNN(const Vec &vec, int limit, std::vector<VecDt> &results) override;
    Status Distance(const std::string &key1, const std::string &key2, double &distance) override;
    Status Load() override;
    Status Build() override;

    int k() const {
        return k_;
    }

    int dim() const {
        return vengine_->dim();
    }

    const std::string& path() const {
        return path_;
    }

  private:
#define KEY_KNN_GRAPH_K "KEY_KNN_GRAPH_K"

    Status WriteK();
    Status LoadK();
    void EncodeKey(const std::string &key, int sequence, std::string &s) const;
    bool DecodeKey(const std::string &s, std::string &key, int &sequence) const;
    void EncodeValue(const std::string &key, double distance, std::string &s) const;
    bool DecodeValue(const std::string &s, std::string &key, double &distance) const;

    std::string path_;
    std::string db_knn_path_;

    leveldb::DB* db_knn_;
    leveldb::DB* db_meta_;
    VEngine* vengine_;
    int k_;
    std::vector<std::string>* all_keys_;
};

} // namespace vectordb

#endif
