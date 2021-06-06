#include "vengine_annoy.h"

namespace vectordb {

VEngineAnnoy::VEngineAnnoy() {
}

VEngineAnnoy::~VEngineAnnoy() {
}

Status
VEngineAnnoy::Open(const Options &options, const std::string &name, VEngine** vengine) {
    return Status::OK();
}

Status
VEngineAnnoy::Put(const WriteOptions &options, const std::string &key, const Vec &result) {
    return Status::OK();
}

Status
VEngineAnnoy::Get(const ReadOptions &options, const std::string &key, Vec &result) {
    return Status::OK();
}

Status
VEngineAnnoy::Delete(const WriteOptions &options, const std::string &key) {
    return Status::OK();
}

Status
VEngineAnnoy::GetKNN(const ReadOptions &options, const std::string &key, std::vector<Vec> &results) {
    return Status::OK();
}

Status
VEngineAnnoy::GetKNN(const ReadOptions &options, const Vec &vec, std::vector<Vec> &results) {
    return Status::OK();
}

Status
VEngineAnnoy::BuildIndex() {
    return Status::OK();
}

bool
VEngineAnnoy::HasIndex() {
    return false;
}


}  // namespace vectordb
