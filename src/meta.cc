#include "meta.h"

namespace vectordb {

Meta::Meta(const std::string &path)
    :path_(path) {
}

Status
Meta::Load() {
    return Status::OK();
}

Status
Meta::Persist() {
    return Status::OK();
}

Status
Meta::Init() {
    return Status::OK();
}

Status
Meta::AddTable(const std::string &name,
               int partition_num,
               int replica_num,
               EngineType engine_type,
               const std::string &path) {
    auto it = tables_.find(name);
    if (it != tables_.end()) {
        std::string msg = name;
        msg.append(" already exist");
        return Status::Corruption(msg);
    }

    auto table = std::make_shared<Table>(name, partition_num, replica_num, engine_type, path);
    tables_.insert(std::pair<std::string, std::shared_ptr<Table>>(name, table));
    return Status::OK();
}

Status
Meta::DropTable(const std::string &name) {
    auto it = tables_.find(name);
    if (it == tables_.end()) {
        std::string msg = name;
        msg.append(" not exist");
        return Status::Corruption(msg);
    }

    tables_.erase(name);
    return Status::OK();
}

Status
Meta::ReplicaName(const std::string &table_name,
                  const std::string &key, std::string &replica_name) const {
    auto it_table = tables_.find(table_name);
    if (it_table == tables_.end()) {
        std::string msg;
        msg.append("table not found:");
        msg.append(table_name);
        return Status::NotFound(msg);
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "%s#partition0#replica0", table_name.c_str());
    replica_name = std::string(buf);
    return Status::OK();
}

}  // namespace vectordb
