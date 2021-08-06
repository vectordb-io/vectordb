#include "node.h"
#include "engine_manager.h"

namespace vectordb {

Status
EngineManager::Init() {
    auto s = Node::GetInstance().mutable_meta().ForEachReplica(std::bind(&EngineManager::LoadEngine, this, std::placeholders::_1));
    if (!s.ok()) {
        std::string msg = "engien manager init error: ";
        msg.append(s.ToString());
        return s;
    }
    return Status::OK();
}

std::shared_ptr<VEngine>
EngineManager::GetVEngine(const std::string &replica_name) const {
    std::unique_lock<std::mutex> guard(mutex_);

    std::shared_ptr<VEngine> ve;
    auto it = vengines_.find(replica_name);
    if (it != vengines_.end()) {
        ve = it->second;
    }
    return ve;
}

void
EngineManager::AddVEngine(const std::string &replica_name, std::shared_ptr<VEngine> ve) {
    std::unique_lock<std::mutex> guard(mutex_);

    auto it = vengines_.find(replica_name);
    assert(it == vengines_.end());
    vengines_.insert(std::pair<std::string, std::shared_ptr<VEngine>>(replica_name, ve));
}

Status
EngineManager::LoadEngine(std::shared_ptr<Replica> r) {
    auto s = LoadEngineByPath(r->path());
    return s;
}

Status
EngineManager::LoadEngineByPath(const std::string &path) {
    auto vengine_sp = std::make_shared<VEngine>(path);
    assert(vengine_sp);

    auto s = vengine_sp->Load();
    if (!s.ok()) {
        std::string msg = "load engine error, path: ";
        msg.append(path).append(", ").append(s.ToString());
        LOG(INFO) << msg;
        return s;
    } else {
        AddVEngine(vengine_sp->replica_name(), vengine_sp);
        std::string msg = "load engine success: ";
        msg.append(vengine_sp->ToString());
    }
    return Status::OK();
}

} // namespace vectordb
