#include <glog/logging.h>
#include "util.h"
#include "coding.h"
#include "vengine.h"

namespace vectordb {

VEngine::VEngine(std::string path,
                 const std::map<std::string, std::string> &indices)
    :path_(path) {
    data_path_ = path_ + "/data";
    index_path_ = path_ + "/index";

    Status s;
    s = Mkdir();
    assert(s.ok());
    s = InitData();
    assert(s.ok());
    s = InitIndices(indices);
    assert(s.ok());
}

VEngine::~VEngine() {
    delete data_;
}

Status
VEngine::Mkdir() {
    if (!util::DirOK(path_)) {
        LOG(INFO) << "mkdir " << path_;
        util::Mkdir(path_);
    }
    if (!util::DirOK(index_path_)) {
        LOG(INFO) << "mkdir " << index_path_;
        util::Mkdir(index_path_);
    }
    return Status::OK();
}

Status
VEngine::InitData() {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, data_path_, &data_);
    assert(status.ok());
    return Status::OK();
}

Status
VEngine::InitIndices(const std::map<std::string, std::string> &indices) {
    for (auto &kv : indices) {
        std::string index_name = kv.first;
        std::string index_type = kv.second;
        std::string index_path = index_path_ + "/" + index_name;
        //auto sp = std::make_shared<VIndex>(index_path, index_type);
        //assert(sp);
        //indices_.insert(std::pair<std::string, std::shared_ptr<VIndex>>(index_name, sp));
    }
    return Status::OK();
}

Status
VEngine::Put(const std::string &key, const Vec &v) {
    std::string value;
    Vec2Str(v, value);

    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    s = data_->Put(write_options, key, value);
    assert(s.ok());

    return Status::OK();
}

Status
VEngine::Get(const std::string &key, Vec &v) {
    bool b;
    leveldb::Status s;
    std::string value;
    s = data_->Get(leveldb::ReadOptions(), key, &value);
    if (s.IsNotFound()) {
        std::string msg = key;
        msg.append(" not found");
        return Status::NotFound(msg);
    }
    assert(s.ok());
    b = Str2Vec(value, v);
    assert(b);
    return Status::OK();
}

Status
VEngine::Delete(const std::string &key) {
    return Status::OK();
}

Status
VEngine::BuildIndex(std::string index_name, std::string index_type) {
}

bool
VEngine::HasIndex() const {
    return indices_.size() > 0;
}

Status
VEngine::Distance(const Vec &v1, const Vec &v2, double &result) const {
}

Status
VEngine::Distance(const std::string &key1, const std::string &key2, double &result) const {
}

Status
VEngine::GetKNN(const std::string &key, std::vector<VecDt> &results, const std::string &index_type) {
}

Status
VEngine::GetKNN(const Vec &vec, std::vector<VecDt> &results, const std::string &index_type) {
}

} // namespace vectordb
