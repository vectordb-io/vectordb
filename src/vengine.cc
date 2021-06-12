#include <glog/logging.h>
#include "util.h"
#include "vengine.h"

namespace vectordb {

VEngine::VEngine(std::string path)
    :path_(path) {
    data_path_ = path_ + "/data";
    index_path_ = path_ + "/index";
}

VEngine::~VEngine() {
    delete data_;
}

Status
VEngine::Init() {
    if (!util::DirOK(path_)) {
        LOG(INFO) << "mkdir " << path_;
        util::Mkdir(path_);
    }
    if (!util::DirOK(index_path_)) {
        LOG(INFO) << "mkdir " << index_path_;
        util::Mkdir(index_path_);
    }

    Status s;
    s = InitData();
    assert(s.ok());
    s = InitIndices();
    assert(s.ok());
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
VEngine::InitIndices() {
    return Status::OK();
}

Status
VEngine::Put(const WriteOptions &options, const std::string &key, const Vec &result) {
}

Status
VEngine::Get(const ReadOptions &options, const std::string &key, Vec &result) {
}

Status
VEngine::Delete(const WriteOptions &options, const std::string &key) {
}

Status
VEngine::GetKNN(const ReadOptions &options, const std::string &key, std::vector<VecDt> &results) {
}

Status
VEngine::GetKNN(const ReadOptions &options, const Vec &vec, std::vector<VecDt> &results) {
}

Status
VEngine::BuildIndex() {
}

bool
VEngine::HasIndex() {
}

} // namespace vectordb
