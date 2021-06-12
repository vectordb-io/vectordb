#include "vengine.h"

namespace vectordb {

Status
VEngine::Open(const Options &options, const std::string &name, VEngine** vengine) {
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
