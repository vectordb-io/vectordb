#ifndef __VECTORDB_VINDEX_ANNOY_H__
#define __VECTORDB_VINDEX_ANNOY_H__

#include <string>
#include <glog/logging.h>
#include "jsonxx/json.hpp"
#include "leveldb/db.h"
#include "kissrandom.h"
#include "annoylib.h"
#include "vec.h"
#include "util.h"
#include "status.h"
#include "vindex.h"

namespace vectordb {

class VEngine;

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

struct AnnoyParam {
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
    VIndexAnnoy(const std::string &path, VEngine* vengine, AnnoyParam *param);    // call Build, path: /tmp/table/partition/replica/index/  will create dir table#annoy#xxx
    VIndexAnnoy(const std::string &path, VEngine* vengine);                       // call Load,  path: /tmp/table/partition/replica/index/table#annoy#xxx
    VIndexAnnoy(const VIndexAnnoy&) = delete;
    VIndexAnnoy& operator=(const VIndexAnnoy&) = delete;
    ~VIndexAnnoy();

    Status GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) override;
    Status GetKNN(const std::vector<float> &vec, int limit, std::vector<VecDt> &results) override;
    Status Distance(const std::string &key1, const std::string &key2, float &distance) override;
    Status Load() override;
    Status Build() override;

    int tree_num() const {
        return tree_num_;
    }

    jsonxx::json64 ToJson() const override;
    std::string ToString() const override;
    std::string ToStringPretty() const override;

  private:
    VIndexParam PrepareVIndexParam(const std::string &path, AnnoyParam *param) {
        VIndexParam vindex_param;
        vindex_param.dim = param->dim;
        vindex_param.index_type = param->index_type;
        vindex_param.distance_type = param->distance_type;
        vindex_param.replica_name = param->replica_name;
        vindex_param.timestamp = param->timestamp;

        std::string table_name;
        int partition_id, replica_id;
        auto b = util::ParseReplicaName(vindex_param.replica_name, table_name, partition_id, replica_id);
        assert(b);
        vindex_param.name = util::IndexName(table_name, vindex_param.index_type, vindex_param.timestamp);

        vindex_param.path = path + "/" + vindex_param.name;

        return vindex_param;
    }

    void InitPath() {
        db_key2id_path_ = path_ + "/key2id";
        db_id2key_path_ = path_ + "/id2key";
        db_meta_path_ = path_ + "/meta";
        annoy_path_ = path_ + "/annoy.idx";
    }

    Status Key2Id(const std::string &key, int &id) const;
    Status Id2Key(int id, std::string &key) const;

    Status CheckParams() const;
    Status Init();
    Status PersistMeta();
    Status LoadMeta();
    Status LoadAnnoy();
    Status ProcResults(const std::vector<int> results, const std::vector<float> distances, std::vector<VecDt> &results_out);
    static float Dt2Cos(float dt);

  private:

    int tree_num_;
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
