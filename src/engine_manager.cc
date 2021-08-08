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

bool
EngineManager::AddVEngine(const std::string &replica_name, std::shared_ptr<VEngine> ve) {
    std::unique_lock<std::mutex> guard(mutex_);

    auto it = vengines_.find(replica_name);
    if (it != vengines_.end()) {
        return false;
    }
    vengines_.insert(std::pair<std::string, std::shared_ptr<VEngine>>(replica_name, ve));
    std::string msg = "add vengine ok: ";
    msg.append(ve->ToString());
    LOG(INFO) << msg;
    return true;
}

bool
EngineManager::AddVEngine2(const std::string &path, const VEngineParam &param) {
    auto vengien_sp = std::make_shared<VEngine>(path, param);
    assert(vengien_sp);

    auto s = vengien_sp->Init();
    if (!s.ok()) {
        std::string msg = "vengine init error: ";
        msg.append(param.replica_name).append(" ").append(path);
        LOG(INFO) << msg;
        return false;
    }

    bool b = AddVEngine(param.replica_name, vengien_sp);
    return b;
}

Status
EngineManager::AddVEngine3(std::shared_ptr<Table> t, std::shared_ptr<Partition> p, std::shared_ptr<Replica> r) {
    std::string path = r->path();
    VEngineParam param;
    param.dim = 10;
    param.replica_name = r->name();
    auto b = AddVEngine2(path, param);
    if (!b) {
        std::string msg = "add engine error: ";
        msg.append(param.replica_name);
        return Status::OtherError(msg);
    }
    return Status::OK();
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

jsonxx::json64
EngineManager::ToJson() const {
    jsonxx::json64 j, jret;
    for (auto &kv : vengines_) {
        j[kv.first] = kv.second->ToJson();
    }
    jret["EngineManager"] = j;
    return jret;
}

std::string
EngineManager::ToString() const {
    return ToJson().dump();
}

std::string
EngineManager::ToStringPretty() const {
    return ToJson().dump(4, ' ');
}

} // namespace vectordb
