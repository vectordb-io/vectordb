#include "engine_manager.h"

namespace vectordb {

Status
EngineManager::Init() {
    return Status::OK();
}

std::shared_ptr<VEngine>
EngineManager::GetVEngine(const std::string &replica_name) const {
    std::shared_ptr<VEngine> ve;
    auto it = vengines_.find(replica_name);
    if (it != vengines_.end()) {
        ve = it->second;
    }
    return ve;
}

void
EngineManager::AddVEngine(const std::string &replica_name, std::shared_ptr<VEngine> ve) {

}

} // namespace vectordb
