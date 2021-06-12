#include "engine_manager.h"

namespace vectordb {

Status
EngineManager::Init() {
    return Status::OK();
}

VEngine*
EngineManager::GetVEngine(const std::string &replica_name) const {
    auto it = vengines_.find(replica_name);
    if (it != vengines_.end()) {
        return it->second;
    }
    return nullptr;
}

void
EngineManager::AddVEngine(const std::string &replica_name, VEngine*) {

}

} // namespace vectordb
