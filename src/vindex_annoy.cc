#include "vengine.h"
#include "coding.h"
#include "vindex_annoy.h"

namespace vectordb {

// call Build, path: /tmp/table/partition/replica/index/  will create dir table#annoy#xxx
VIndexAnnoy::VIndexAnnoy(const std::string &path, VEngine* vengine, AnnoyParam *param)
    :VIndex(PrepareVIndexParam(path, param)),
     tree_num_(param->tree_num),
     vengine_(vengine),
     db_key2id_(nullptr),
     db_id2key_(nullptr),
     db_meta_(nullptr),
     annoy_index_(nullptr) {

    InitPath();
}

// call Load, path: /tmp/table/partition/replica/index/table#annoy#xxx
VIndexAnnoy::VIndexAnnoy(const std::string &path, VEngine* vengine)
    :vengine_(vengine),
     db_key2id_(nullptr),
     db_id2key_(nullptr),
     db_meta_(nullptr),
     annoy_index_(nullptr) {

    path_ = path;
    InitPath();
}

VIndexAnnoy::~VIndexAnnoy() {
    delete db_key2id_;
    delete db_id2key_;
    delete db_meta_;
    delete annoy_index_;
}

Status
VIndexAnnoy::GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) {
    int search_k = (2 * limit > 20) ? (2 * limit) : 20;
    int id;

    if (limit > 0) {
        auto s = Key2Id(key, id);
        if (!s.ok()) {
            std::string msg = "getknn_by_key Key2Id error ";
            msg.append(key).append(" ").append(s.ToString());
            return s;
        }

        std::vector<int> result;
        std::vector<float> distances;

        annoy_index_->get_nns_by_item(id, limit, search_k, &result, &distances);
        assert(result.size() == distances.size());

        s = ProcResults(result, distances, results);
        if (!s.ok()) {
            std::string msg = ToString() + " getknn by key error: " + s.ToString();
            LOG(INFO) << msg;
            return s;
        }
    }
    return Status::OK();
}

Status
VIndexAnnoy::GetKNN(const std::vector<float> &vec, int limit, std::vector<VecDt> &results) {
    int search_k = (2 * limit > 20) ? (2 * limit) : 20;

    if (static_cast<int>(vec.size()) != dim_) {
        return Status::OtherError("dim not equal");
    }

    if (limit > 0) {
        std::vector<int> result;
        std::vector<float> distances;

        annoy_index_->get_nns_by_vector(vec.data(), limit, search_k, &result, &distances);
        assert(result.size() == distances.size());

        auto s = ProcResults(result, distances, results);
        if (!s.ok()) {
            std::string msg = ToString() + " getknn by vec error: " + s.ToString();
            LOG(INFO) << msg;
            return s;
        }
    }
    return Status::OK();
}

Status
VIndexAnnoy::ProcResults(const std::vector<int> results, const std::vector<float> distances, std::vector<VecDt> &results_out) {
    assert(results.size() == distances.size());
    results_out.clear();

    for (size_t i = 0; i < results.size(); ++i) {
        std::string find_key;
        auto s = Id2Key(results[i], find_key);
        if (!s.ok()) {
            std::string msg = "ProcResults Id2Key error ";
            msg.append(find_key).append(" ").append(s.ToString());
            return s;
        }

        VecObj vo;
        s = vengine_->Get(find_key, vo);
        if (!s.ok()) {
            std::string msg = "ProcResults get vector object error ";
            msg.append(find_key).append(" ").append(s.ToString());
            return s;
        }
        assert(find_key == vo.key());

        VecDtParam param;
        param.key = vo.key();

        if (distance_type_ == VINDEX_DISTANCE_TYPE_COSINE) {
            param.distance = Dt2Cos(distances[i]);

        } else {
            param.distance = distances[i];
        }

        param.attach_value1 = vo.attach_value1();
        param.attach_value2 = vo.attach_value2();
        param.attach_value3 = vo.attach_value3();

        VecDt vdt(param);
        results_out.push_back(vdt);
        std::string log_str = "ProcResults one: " + vdt.ToString();
        LOG(INFO) << log_str;
    }

    if (distance_type_ == VINDEX_DISTANCE_TYPE_COSINE) {
        std::sort(results_out.begin(), results_out.end(), std::greater<VecDt>());
    } else {
        std::sort(results_out.begin(), results_out.end(), std::less<VecDt>());
    }

    return Status::OK();
}

/*
Status
VIndexAnnoy::Distance(const std::string &key1, const std::string &key2, float &distance) {
    int id1, id2;
    auto s = Key2Id(key1, id1);
    if (!s.ok()) {
        return s;
    }
    s = Key2Id(key2, id2);
    if (!s.ok()) {
        return s;
    }

    distance = annoy_index_->get_distance(id1, id2);
    if (distance_type_ == VINDEX_DISTANCE_TYPE_COSINE) {
        distance = Dt2Cos(distance);
    }

    return Status::OK();
}
*/

Status
VIndexAnnoy::CheckParams() const {
    if (index_type_ != VINDEX_TYPE_ANNOY) {
        return Status::OtherError("param index_type error");

    } else if (distance_type_ != VINDEX_DISTANCE_TYPE_COSINE &&
               distance_type_ != VINDEX_DISTANCE_TYPE_INNER_PRODUCT &&
               distance_type_ != VINDEX_DISTANCE_TYPE_EUCLIDEAN) {
        return Status::OtherError("param distance_type error");
    }

    return Status::OK();
}

Status
VIndexAnnoy::Init() {
    auto s = CheckParams();
    if (!s.ok()) {
        return s;
    }

    if (util::DirOK(path_)) {
        std::string msg = "vindex_annoy init error, dir already exist: ";
        msg.append(path_);
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    util::RecurMakeDir(path_);
    if (!util::DirOK(path_)) {
        std::string msg = "vindex_annoy dir error: ";
        msg.append(path_);
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status ls;

    ls = leveldb::DB::Open(options, db_key2id_path_, &db_key2id_);
    assert(ls.ok());
    ls = leveldb::DB::Open(options, db_id2key_path_, &db_id2key_);
    assert(ls.ok());
    ls = leveldb::DB::Open(options, db_meta_path_, &db_meta_);
    assert(ls.ok());

    s = PersistMeta();
    if (!s.ok()) {
        std::string msg = "vindex_annoy persist meta error: ";
        msg.append(s.Msg());
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
VIndexAnnoy::Load() {
    if (!util::DirOK(path_)) {
        std::string msg = "vindex_annoy load error, dir not exist: ";
        msg.append(path_);
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    auto s = LoadMeta();
    if (!s.ok()) {
        std::string msg = replica_name_;
        msg.append(" vindex_annoy load meta error");
        LOG(INFO) << msg;
        return s;
    }

    leveldb::Options options;
    //options.create_if_missing = true;
    leveldb::Status ls;

    ls = leveldb::DB::Open(options, db_key2id_path_, &db_key2id_);
    assert(ls.ok());
    ls = leveldb::DB::Open(options, db_id2key_path_, &db_id2key_);
    assert(ls.ok());

    s = LoadAnnoy();
    if (!s.ok()) {
        std::string msg = replica_name_;
        msg.append(" vindex_annoy load annoy error");
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
VIndexAnnoy::Build() {
    auto s = Init();
    if (!s.ok()) {
        return s;
    }

    annoy_index_ = AnnoyFactory::Create(distance_type_, vengine_->dim());
    if (!annoy_index_) {
        std::string msg = replica_name_;
        msg.append(" vindex_annoy factory build error");
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    int annoy_index_id = 0;
    leveldb::Iterator* it = vengine_->mutable_db_data()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        std::string value = it->value().ToString();

        VecObj vo;
        bool b = coding::Str2VecObj(value, vo);
        assert(b);

        if (vo.vec().dim() != dim_) {
            return Status::OtherError("dim not equal");
        }

        std::vector<float> arr;
        for (int j = 0; j < vo.vec().dim(); ++j) {
            arr.push_back(vo.vec().data()[j]);
        }
        const float *parr = &(*arr.begin());
        annoy_index_->add_item(annoy_index_id, parr);

        std::string id_string;
        coding::Int322Str(annoy_index_id, id_string);

        leveldb::Status s;
        leveldb::WriteOptions write_options;
        write_options.sync = true;

        s = db_id2key_->Put(write_options, id_string, key);
        assert(s.ok());
        //LOG(INFO) << "build index id2key: " << annoy_index_id << " " << key;

        s = db_key2id_->Put(write_options, key, id_string);
        assert(s.ok());
        //LOG(INFO) << "build index key2id: " << key << " " << annoy_index_id;

        annoy_index_id++;
        if (annoy_index_id % 500 == 0) {
            char log_buf[128];
            snprintf(log_buf, sizeof(log_buf), "build index: annoy_index_id: %d", annoy_index_id);
            LOG(INFO) << log_buf;
        }
    }
    if (!it->status().ok()) {
        std::string msg = "build index iterator error: ";
        msg.append(it->status().ToString());
        return Status::OtherError(msg);
    }
    delete it;

    //annoy_index_.build(2 * dim());
    annoy_index_->build(tree_num_);
    LOG(INFO) << name_ << " build tree finish";

    annoy_index_->save(annoy_path_.c_str());
    LOG(INFO) << name_ << " save tree finish";

    return Status::OK();
}

Status
VIndexAnnoy::Key2Id(const std::string &key, int &id) const {
    std::string value;
    leveldb::Status ls;
    ls = db_key2id_->Get(leveldb::ReadOptions(), key, &value);
    if (!ls.ok()) {
        return Status::OtherError(ls.ToString());
    }
    auto b = coding::Str2Int32(value, id);
    if (!b) {
        return Status::OtherError("Str2Int32 error");
    }
    return Status::OK();
}

Status
VIndexAnnoy::Id2Key(int id, std::string &key) const {
    std::string id_string;
    leveldb::Status ls;
    coding::Int322Str(id, id_string);
    ls = db_id2key_->Get(leveldb::ReadOptions(), id_string, &key);
    if (!ls.ok()) {
        return Status::OtherError(ls.ToString());
    }
    return Status::OK();
}

jsonxx::json64
VIndexAnnoy::ToJson() const {
    jsonxx::json64 j, jret;
    j["dim"] = dim_;
    j["index_type"] = index_type_;
    j["distance_type"] = distance_type_;
    j["name"] = name_;
    j["replica_name"] = replica_name_;
    j["timestamp"] = timestamp_;
    j["timestamp_str"] = timestamp_str();
    j["tree_num"] = tree_num_;
    j["path"] = path_;
    j["db_key2id_path"] = db_key2id_path_;
    j["db_id2key_path"] = db_id2key_path_;
    j["db_meta_path"] = db_meta_path_;
    j["annoy_path"] = annoy_path_;
    jret["VIndexAnnoy"] = j;
    return jret;
}

std::string
VIndexAnnoy::ToString() const {
    return ToJson().dump();
}

std::string
VIndexAnnoy::ToStringPretty() const {
    return ToJson().dump(4, ' ');
}

Status
VIndexAnnoy::PersistMeta() {
    vectordb_rpc::AnnoyMeta pb;
    pb.set_dim(dim_);
    pb.set_index_type(index_type_);
    pb.set_distance_type(distance_type_);
    pb.set_name(name_);
    pb.set_replica_name(replica_name_);
    pb.set_timestamp(timestamp_);
    pb.set_tree_num(tree_num_);

    std::string buf;
    bool b = pb.SerializeToString(&buf);
    assert(b);

    leveldb::Status ls;
    leveldb::WriteOptions wo;
    wo.sync = true;
    ls = db_meta_->Put(wo, KEY_META_ANNOY_INDEX, buf);
    assert(ls.ok());

    return Status::OK();
}

Status
VIndexAnnoy::LoadMeta() {
    leveldb::Options options;
    //options.create_if_missing = true;
    leveldb::Status ls = leveldb::DB::Open(options, db_meta_path_, &db_meta_);
    if (!ls.ok()) {
        std::string msg = "vindex_annoy load meta error: ";
        msg.append(ls.ToString());
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    std::string buf;
    ls = db_meta_->Get(leveldb::ReadOptions(), KEY_META_ANNOY_INDEX, &buf);
    if (ls.ok()) {
        vectordb_rpc::AnnoyMeta pb;
        bool b = pb.ParseFromString(buf);
        if (!b) {
            std::string msg = "vindex_annoy load meta error, ParseFromString";
            LOG(INFO) << msg;
            return Status::OtherError(msg);
        }

        dim_ = pb.dim();
        index_type_ = pb.index_type();
        distance_type_ = pb.distance_type();
        name_ = pb.name();
        replica_name_ = pb.replica_name();
        timestamp_ = pb.timestamp();
        tree_num_ = pb.tree_num();

    } else {
        std::string msg = "vindex_annoy load meta error: ";;
        msg.append(ls.ToString());
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
VIndexAnnoy::LoadAnnoy() {
    annoy_index_ = AnnoyFactory::Create(distance_type_, dim_);
    auto b = annoy_index_->load(annoy_path_.c_str());
    if (!b) {
        std::string msg = name_;
        msg.append(" LoadAnnoy error");
        return Status::OtherError(msg);
    }

    return Status::OK();
}

float
VIndexAnnoy::Dt2Cos(float dt) {
    return 1 - dt / 2;
}

Status
VIndexAnnoy::Distance(const std::vector<float> &vec1, const std::vector<float> &vec2, const std::string &distance_type, float &distance) {
    if (vec1.size() != vec2.size()) {
        return Status::OtherError("dim not equal");
    }

    AnnoyIndexInterface<int, float> *annoy_index = AnnoyFactory::Create(distance_type, vec1.size());
    if (annoy_index) {
        return Status::OtherError("create annoy index error");
    }

    const float *p1= &(*vec1.begin());
    const float *p2= &(*vec2.begin());

    annoy_index->add_item(0, p1);
    annoy_index->add_item(1, p2);
    distance = annoy_index->get_distance(0, 1);
    if (distance_type == VINDEX_DISTANCE_TYPE_COSINE) {
        distance = Dt2Cos(distance);
    }
    delete annoy_index;
    return Status::OK();
}

} // namespace vectordb
