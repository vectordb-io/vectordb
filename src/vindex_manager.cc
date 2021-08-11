#include "util.h"
#include "vindex_manager.h"

namespace vectordb {

VIndexManager::VIndexManager(const std::string path, VEngine *vengine)
    :path_(path), vengine_(vengine) {
}

VIndexManager::~VIndexManager() {

}

Status
VIndexManager::Init() {
    if (util::DirOK(path_)) {
        std::string msg = "vindex_manager init error, dir already exist: ";
        msg.append(path_);
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    util::RecurMakeDir(path_);
    if (!util::DirOK(path_)) {
        std::string msg = "vindex_manager dir error: ";
        msg.append(path_);
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
VIndexManager::Add(const std::string &name, std::shared_ptr<VIndex> index) {
    std::unique_lock<std::mutex> guard(mutex_);
    indices_by_name_.insert(std::pair<std::string, std::shared_ptr<VIndex>>(index->name(), index));
    indices_by_time_.insert(std::pair<time_t, std::shared_ptr<VIndex>>(index->timestamp(), index));
    return Status::OK();
}

Status
VIndexManager::Del(const std::string &name) {
    std::unique_lock<std::mutex> guard(mutex_);

    return Status::OK();
}

Status
VIndexManager::ForEachIndex(std::function<Status(std::shared_ptr<VIndex>)> func) {
    std::unique_lock<std::mutex> guard(mutex_);
    for (auto &kv : indices_by_name_) {
        auto s = func(kv.second);
        if (!s.ok()) {
            return s;
        }
    }

    return Status::OK();
}

jsonxx::json64
VIndexManager::ToJson() const {
    std::unique_lock<std::mutex> guard(mutex_);

    jsonxx::json64 j, jret;
    for (auto &kv : indices_by_name_) {
        j[kv.first] = kv.second->ToJson();
    }
    jret["VIndexManager"] = j;
    return jret;
}

std::string
VIndexManager::ToString() const {
    return ToJson().dump();
}

std::string
VIndexManager::ToStringPretty() const {
    return ToJson().dump(4, ' ');
}

} // namespace vectordb
