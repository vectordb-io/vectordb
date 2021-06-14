#include "node.h"
#include "engine_manager.h"

namespace vectordb {

Status
EngineManager::Init() {
    for (auto &table_kv : Node::GetInstance().meta().tables()) {
        if (table_kv.second->engine_type() == VECTOR_ENGINE) {
            for (auto &partition_kv : table_kv.second->partitions()) {
                for (auto &replica_kv : partition_kv.second->replicas()) {
                    auto replica_sp = replica_kv.second;
                    std::map<std::string, std::string> empty_indices;
                    auto vengine = std::make_shared<VEngine>(replica_sp->path(), table_kv.second->dim(), empty_indices);
                    assert(vengine);
                    auto s = vengine->Init();
                    assert(s.ok());
                    Node::GetInstance().mutable_engine_manager().AddVEngine(replica_sp->name(), vengine);
                }
            }
        }
    }
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
    auto it = vengines_.find(replica_name);
    assert(it == vengines_.end());
    vengines_.insert(std::pair<std::string, std::shared_ptr<VEngine>>(replica_name, ve));
}

} // namespace vectordb
