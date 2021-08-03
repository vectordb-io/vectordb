#ifndef __VECTORDB_VINDEX_ANNOY_H__
#define __VECTORDB_VINDEX_ANNOY_H__

#include <string>
#include <glog/logging.h>
#include "leveldb/db.h"
#include "kissrandom.h"
#include "annoylib.h"
#include "vec.h"
#include "status.h"
#include "vindex.h"

namespace vectordb {

class AnnoyIndexFactory {
  public:
    static AnnoyIndexInterface<int, double>*
    Create(const std::string &distance_type, int dim) {
        AnnoyIndexInterface<int, double> *annoy_index = nullptr;
        if (distance_type == "cosine") {
            annoy_index = new AnnoyIndex<int, double, Angular, Kiss32Random, AnnoyIndexMultiThreadedBuildPolicy>(dim);

        } else if (distance_type == "inner_product") {
            annoy_index = new AnnoyIndex<int, double, DotProduct, Kiss32Random, AnnoyIndexMultiThreadedBuildPolicy>(dim);

        } else if (distance_type == "euclidean") {
            annoy_index = new AnnoyIndex<int, double, Euclidean, Kiss32Random, AnnoyIndexMultiThreadedBuildPolicy>(dim);

        } else {
            std::string msg = "create annoy index error, unknown distance_type ";
            msg.append(distance_type);
            LOG(INFO) << msg;
        }
        return annoy_index;
    }
};

class AnnoyParam {
  public:
    std::string distance_type;

    void SerializeToString(std::string &s) const {

    }

    Status ParseFromString(const std::string &s) {

    }
};

class VIndexAnnoy : public VIndex {
  public:
#define KEY_META_ANNOY_INDEX "KEY_META_ANNOY_INDEX"

  public:
    VIndexAnnoy(const std::string &path, VEngine* vengine, AnnoyParam* param);  // call Build
    VIndexAnnoy(const std::string &path, VEngine* vengine);                     // call Load
    VIndexAnnoy(const VIndexAnnoy&) = delete;
    VIndexAnnoy& operator=(const VIndexAnnoy&) = delete;
    ~VIndexAnnoy();

    Status GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) override;
    Status GetKNN(const Vec &vec, int limit, std::vector<VecDt> &results) override;
    Status Distance(const std::string &key1, const std::string &key2, double &distance) override;
    Status Load() override;
    Status Build() override;

    int dim() const {
        return vengine_->dim();
    }

    const std::string& path() const {
        return path_;
    }

  private:
    void InitPath() {
        db_key2id_path_ = path_ + "/key2id";
        db_id2key_path_ = path_ + "/id2key";
        db_meta_path_ = path_ + "/meta";
        annoy_path_ = path_ + "/annoy.idx";
    }

  private:
    bool Key2Id(const std::string &key, int &id) const;
    bool Id2Key(int id, std::string &key) const;

    std::string distance_type_;
    std::string path_;
    std::string db_key2id_path_;
    std::string db_id2key_path_;
    std::string db_meta_path_;
    std::string annoy_path_;

    leveldb::DB* db_key2id_;
    leveldb::DB* db_id2key_;
    leveldb::DB* db_meta_;
    VEngine* vengine_;

    AnnoyIndexInterface<int, double> *annoy_index_;
};

} // namespace vectordb

#endif
