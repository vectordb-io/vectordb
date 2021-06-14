#ifndef __VECTORDB_VINDEX_ANNOY_H__
#define __VECTORDB_VINDEX_ANNOY_H__

#include <string>
#include "leveldb/db.h"
#include "kissrandom.h"
#include "annoylib.h"
#include "vec.h"
#include "status.h"
#include "vindex.h"

namespace vectordb {

class VIndexAnnoy : public VIndex {
  public:
    VIndexAnnoy(const std::string &path, VEngine* vengine);
    VIndexAnnoy(const VIndexAnnoy&) = delete;
    VIndexAnnoy& operator=(const VIndexAnnoy&) = delete;
    ~VIndexAnnoy();

    Status GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) override;
    Status GetKNN(const Vec &vec, int limit, std::vector<VecDt> &results) override;
    Status Distance(const std::string &key1, const std::string &key2, double &distance) override;
    Status Init() override;

    int dim() const {
        return vengine_->dim();
    }

    const std::string& path() const {
        return path_;
    }

  private:
    Status Load();
    Status Build();
    bool Key2Id(const std::string &key, int &id) const;
    bool Id2Key(int id, std::string key) const;

    std::string path_;
    std::string db_key2id_path_;
    std::string db_id2key_path_;
    std::string annoy_path_;

    leveldb::DB* db_key2id_;
    leveldb::DB* db_id2key_;
    VEngine* vengine_;

    AnnoyIndex<int, double, Angular, Kiss32Random> annoy_index_;
};

} // namespace vectordb

#endif
