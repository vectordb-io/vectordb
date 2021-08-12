#include <glog/logging.h>
#include "vectordb_rpc.pb.h"
#include "util.h"
#include "coding.h"
#include "vengine.h"
#include "vindex_annoy.h"
#include "vindex_knn_graph.h"

namespace vectordb {

VEngine::VEngine(std::string path, const VEngineParam& param)
    :path_(path),
     data_path_(path_ + "/data"),
     meta_path_(path_ + "/meta"),
     index_path_(path_ + "/index"),
     dim_(param.dim),
     replica_name_(param.replica_name),
     db_data_(nullptr),
     db_meta_(nullptr),
     vindex_manager_(index_path_, this) {
}

VEngine::VEngine(std::string path)
    :path_(path),
     data_path_(path_ + "/data"),
     meta_path_(path_ + "/meta"),
     index_path_(path_ + "/index"),
     db_data_(nullptr),
     db_meta_(nullptr),
     vindex_manager_(index_path_, this) {
}

VEngine::~VEngine() {
    delete db_data_;
    delete db_meta_;
}

Status
VEngine::Init() {
    if (util::DirOK(path_)) {
        std::string msg = "vengine init error, dir already exist: ";
        msg.append(path_);
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    util::RecurMakeDir(path_);
    if (!util::DirOK(path_)) {
        std::string msg = "vengine dir error: ";
        msg.append(path_);
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    auto s = vindex_manager_.Init();
    if (!s.ok()) {
        std::string msg = "vengine init error, :vindex_manager init error: ";
        msg.append(s.Msg());
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status ls;

    ls = leveldb::DB::Open(options, meta_path_, &db_meta_);
    assert(ls.ok());
    ls = leveldb::DB::Open(options, data_path_, &db_data_);
    assert(ls.ok());

    s = PersistMeta();
    if (!s.ok()) {
        std::string msg = "vengine persist meta error: ";
        msg.append(s.Msg());
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
VEngine::Load() {
    if (!util::DirOK(path_)) {
        std::string msg = "vengine load error, dir not exist: ";
        msg.append(path_);
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    auto s = LoadMeta();
    if (!s.ok()) {
        std::string msg = replica_name_;
        msg.append(" vengine load meta error");
        LOG(INFO) << msg;
        return s;
    }

    s = LoadData();
    if (!s.ok()) {
        std::string msg = replica_name_;
        msg.append(" vengine load data error");
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    s = LoadIndex();
    if (!s.ok()) {
        std::string msg = replica_name_;
        msg.append(" vengine load index error: ").append(s.ToString());
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
VEngine::PersistMeta() {
    vectordb_rpc::VEngineMeta pb;
    pb.set_dim(dim_);
    pb.set_replica_name(replica_name_);

    /*
    for (auto &index_name : indices_) {
        pb.add_index_names(index_name);
    }
    */

    std::string buf;
    bool ret = pb.SerializeToString(&buf);
    assert(ret);

    leveldb::Status ls;
    leveldb::WriteOptions wo;
    wo.sync = true;
    ls = db_meta_->Put(wo, KEY_VENGINE_META_PERSIST, buf);
    assert(ls.ok());

    return Status::OK();
}

Status
VEngine::LoadMeta() {
    leveldb::Options options;
    //options.create_if_missing = true;
    leveldb::Status ls = leveldb::DB::Open(options, meta_path_, &db_meta_);
    if (!ls.ok()) {
        std::string msg = "load meta error: ";
        msg.append(ls.ToString());
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    std::string buf;
    ls = db_meta_->Get(leveldb::ReadOptions(), KEY_VENGINE_META_PERSIST, &buf);
    if (ls.ok()) {
        vectordb_rpc::VEngineMeta pb;
        bool b = pb.ParseFromString(buf);
        if (!b) {
            std::string msg = "load meta error, ParseFromString";
            LOG(INFO) << msg;
            return Status::OtherError(msg);
        }

        dim_ = pb.dim();
        replica_name_ = pb.replica_name();

        /*
        for (int i = 0; i < pb.index_names_size(); ++i) {
            indices_.push_back(pb.index_names(i));
        }
        */

    } else {
        std::string msg = "load meta error: ";;
        msg.append(ls.ToString());
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
VEngine::LoadData() {
    leveldb::Options options;
    //options.create_if_missing = true;
    leveldb::Status ls = leveldb::DB::Open(options, data_path_, &db_data_);
    if (!ls.ok()) {
        std::string msg = "load data error: ";
        msg.append(ls.ToString());
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
VEngine::LoadIndex() {

    std::vector<std::string> children_paths;
    std::vector<std::string> children_names;
    auto b = util::ChildrenOfDir(index_path_, children_paths, children_names);;
    if (!b) {
        std::string msg = "load index ChildrenOfDir error: " + index_path_;
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    for (auto &index_name : children_names) {
        std::string table_name;
        std::string index_type;
        time_t timestamp;
        b = util::ParseIndexName(index_name, table_name, index_type, timestamp);
        if (!b) {
            std::string msg = "load index ParseIndexName error: ";
            msg.append(index_name);
            LOG(INFO) << msg;
            return Status::OtherError(msg);
        }

        std::string index_path = index_path_ + "/" + index_name;
        auto index_sp = VIndexFactory::Create(index_type, index_path, this);
        if (!index_sp) {
            std::string msg = "load index VIndexFactory::Create error: ";
            msg.append(index_name);
            LOG(INFO) << msg;
            return Status::OtherError(msg);
        }

        auto s = index_sp->Load();
        if (!s.ok()) {
            std::string msg = "load index VIndexFactory::Create error: ";
            msg.append(index_name).append(", ").append(s.Msg());
            LOG(INFO) << msg;
            return Status::OtherError(msg);

        }

        s = vindex_manager_.Add(index_sp);
        if (!s.ok()) {
            std::string msg = "load index vindex_manager_.Add error: ";
            msg.append(index_name).append(", ").append(s.Msg());
            LOG(INFO) << msg;
            return Status::OtherError(msg);
        }

        std::string msg = "load index ok: " + index_sp->ToString();
        LOG(INFO) << msg;
    }
    return Status::OK();
}

Status
VEngine::Put(const std::string &key, const VecObj &vo) {
    if (vo.vec().dim() != dim_) {
        return Status::OtherError("dim not equal");
    }

    std::string value;
    vo.SerializeToString(value);

    leveldb::Status ls;
    leveldb::WriteOptions wo;
    wo.sync = true;
    ls = db_data_->Put(wo, key, value);
    if (!ls.ok()) {
        std::string msg = "put vec to db error: ";
        msg.append(ls.ToString());
        return Status::OtherError(msg);
    }
    return Status::OK();
}

Status
VEngine::Get(const std::string &key, VecObj &vo) const {
    leveldb::Status ls;
    std::string value;
    ls = db_data_->Get(leveldb::ReadOptions(), key, &value);
    if (ls.IsNotFound()) {
        std::string msg = key;
        msg.append(" not found");
        LOG(INFO) << msg;
        return Status::NotFound(msg);
    }

    if (!ls.ok()) {
        std::string msg = "get vec from db error, key:";
        msg.append(key).append(", ").append(ls.ToString());
        LOG(INFO) << msg;
        return Status::NotFound(msg);
    }

    auto b = vo.ParseFromString(value);
    if (!b) {
        std::string msg = "get vec from db, parse from string error, key:";
        msg.append(key);
        LOG(INFO) << msg;
        return Status::NotFound(msg);
    }

    return Status::OK();
}

Status
VEngine::Delete(const std::string &key) {
    return Status::OK();
}

bool
VEngine::HasIndex() const {
    return vindex_manager_.HasIndex();
}

Status
VEngine::AddIndex(std::string index_type, void *param) {
    std::shared_ptr<VIndex> index_sp;
    index_sp = VIndexFactory::Create(index_type, index_path_, this, param);
    if (!index_sp) {
        std::string msg = replica_name_ + " VIndexFactory::Create error";
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    auto s = index_sp->Build();
    if (!s.ok()) {
        std::string msg = replica_name_ + " build index error: " + s.Msg();
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    s = vindex_manager_.Add(index_sp);
    if (!s.ok()) {
        std::string msg = replica_name_ + " vindex_manager_.Add, " + index_sp->name() + " " + s.Msg();
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    std::string msg = replica_name_ + " VEngine::AddIndex " + index_sp->name() + " ok";
    LOG(INFO) << msg;
    return Status::OK();
}

Status
VEngine::GetKNN(const std::string &key, int limit, std::vector<VecDt> &results, const std::string &index_name) {
    std::shared_ptr<VIndex> index_sp;
    if (index_name == "default") {
        index_sp = vindex_manager_.GetNewestIndex();
    } else {
        index_sp = vindex_manager_.GetIndexByName(index_name);
    }

    if (!index_sp) {
        std::string msg = "index not found, " + index_name;
        return Status::OtherError(msg);
    }

    auto s = index_sp->GetKNN(key, limit, results);
    if (!s.ok()) {
        return s;
    }

    return Status::OK();
}

Status
VEngine::GetKNN(const std::vector<float> &vec, int limit, std::vector<VecDt> &results, const std::string &index_name) {
    std::shared_ptr<VIndex> index_sp;
    if (index_name == "default") {
        index_sp = vindex_manager_.GetNewestIndex();
    } else {
        index_sp = vindex_manager_.GetIndexByName(index_name);
    }

    if (!index_sp) {
        std::string msg = "index not found, " + index_name;
        return Status::OtherError(msg);
    }

    auto s = index_sp->GetKNN(vec, limit, results);
    if (!s.ok()) {
        return s;
    }

    return Status::OK();
}

jsonxx::json64
VEngine::ToJson() const {
    jsonxx::json64 j, jret;
    j["dim"] = dim_;
    j["replica_name"] = replica_name_;
    j["path"] = path_;
    j["data_path"] = data_path_;
    j["meta_path"] = meta_path_;
    j["index_path"] = index_path_;
    j["index"] = vindex_manager_.ToJson();
    jret["VEngine"] = j;
    return jret;
}

std::string
VEngine::ToString() const {
    return ToJson().dump();
}

std::string
VEngine::ToStringPretty() const {
    return ToJson().dump(4, ' ');
}


} // namespace vectordb
