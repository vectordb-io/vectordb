#ifndef __VECTORDB_VINDEX_ANNOY_H__
#define __VECTORDB_VINDEX_ANNOY_H__

#include <string>
#include <glog/logging.h>
#include "jsonxx/json.hpp"
#include "leveldb/db.h"
#include "kissrandom.h"
#include "annoylib.h"
#include "vec.h"
#include "status.h"
#include "vindex.h"

namespace vectordb {

class AnnoyFactory {
  public:
    static AnnoyIndexInterface<int, float>*
    Create(const std::string &distance_type, int dim) {
        AnnoyIndexInterface<int, float> *annoy_index = nullptr;
        if (distance_type == VINDEX_DISTANCE_TYPE_COSINE) {
            annoy_index = new AnnoyIndex<int, float, Angular, Kiss32Random, AnnoyIndexMultiThreadedBuildPolicy>(dim);

        } else if (distance_type == VINDEX_DISTANCE_TYPE_INNER_PRODUCT) {
            annoy_index = new AnnoyIndex<int, float, DotProduct, Kiss32Random, AnnoyIndexMultiThreadedBuildPolicy>(dim);

        } else if (distance_type == VINDEX_DISTANCE_TYPE_EUCLIDEAN) {
            annoy_index = new AnnoyIndex<int, float, Euclidean, Kiss32Random, AnnoyIndexMultiThreadedBuildPolicy>(dim);

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
    int dim;
    std::string index_type;
    std::string distance_type;
    std::string replica_name;
    time_t timestamp;
    int tree_num;
};

class VIndexAnnoy : public VIndex {
  public:
#define KEY_META_ANNOY_INDEX "KEY_META_ANNOY_INDEX"

  public:
    VIndexAnnoy(const std::string &path, VEngine* vengine, const AnnoyParam &param);  // call Build
    VIndexAnnoy(const std::string &path, VEngine* vengine);                           // call Load
    VIndexAnnoy(const VIndexAnnoy&) = delete;
    VIndexAnnoy& operator=(const VIndexAnnoy&) = delete;
    ~VIndexAnnoy();

    Status GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) override;
    Status GetKNN(const Vec &vec, int limit, std::vector<VecDt> &results) override;
    Status Distance(const std::string &key1, const std::string &key2, float &distance) override;
    Status Load() override;
    Status Build() override;

    int dim() const {
        return dim_;
    }

    std::string index_type() const {
        return index_type_;
    }

    std::string distance_type() const {
        return distance_type_;
    }

    std::string name() const {
        return name_;
    }

    std::string replica_name() const {
        return replica_name_;
    }

    time_t timestamp() const {
        return timestamp_;
    }

    std::string timestamp_str() const {
        std::string s = util::LocalTimeString(timestamp_);
        return s;
    }

    int tree_num() const {
        return tree_num_;
    }

    std::string path() const {
        return path_;
    }

    jsonxx::json64 ToJson() const;
    std::string ToString() const;
    std::string ToStringPretty() const;

  private:
    void InitPath() {
        db_key2id_path_ = path_ + "/key2id";
        db_id2key_path_ = path_ + "/id2key";
        db_meta_path_ = path_ + "/meta";
        annoy_path_ = path_ + "/annoy.idx";
    }

    Status Key2Id(const std::string &key, int &id) const;
    Status Id2Key(int id, std::string &key) const;

    Status Init();
    Status PersistMeta();
    Status LoadMeta();
    Status LoadAnnoy();
    Status ProcResults(const std::vector<int> results, const std::vector<float> distances, std::vector<VecDt> &results_out);
    static float Dt2Cos(float dt);

  private:
    int dim_;
    std::string index_type_;
    std::string distance_type_;
    std::string name_;
    std::string replica_name_;
    time_t timestamp_;
    int tree_num_;
    std::string path_;
    VEngine* vengine_;

    std::string db_key2id_path_;
    std::string db_id2key_path_;
    std::string db_meta_path_;
    std::string annoy_path_;

    leveldb::DB* db_key2id_;
    leveldb::DB* db_id2key_;
    leveldb::DB* db_meta_;

    AnnoyIndexInterface<int, float> *annoy_index_;
};

} // namespace vectordb

#endif
