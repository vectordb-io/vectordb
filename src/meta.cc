#include <glog/logging.h>
#include "util.h"
#include "coding.h"
#include "meta.h"

namespace vectordb {

std::string
EngineTypeToString(EngineType e) {
    if (e == kKVEngine) {
        return "kKVEngine";

    } else if (e == kVectorEngine) {
        return "kVectorEngine";

    } else if (e == kGraphEngine) {
        return "kGraphEngine";

    }
    return "error engine";
}

EngineType StringToEngineType(const std::string &s) {
    std::string engine_type = s;
    util::ToLower(engine_type);
    if (s == "kv") {
        return kKVEngine;

    } else if (s == "vector") {
        return kVectorEngine;

    } else if (s == "graph") {
        return kGraphEngine;

    } else {
        return kErrorEngine;
    }
}

Meta::Meta(const std::string &path)
    :path_(path) {
}

Status
Meta::Load() {
    bool b;
    leveldb::Status s;
    std::string value;
    std::vector<std::string> table_names;

    s = db_->Get(leveldb::ReadOptions(), META_PERSIST_KEY_TABLES, &value);
    if (s.IsNotFound()) {
        return Status::OK();
    }
    assert(s.ok());

    b = Str2TableNames(value, table_names);
    assert(b);

    for (auto &table_name : table_names) {
        LOG(INFO) << "loading meta... table:" << table_name;
        s = db_->Get(leveldb::ReadOptions(), table_name, &value);
        assert(s.ok());

        Table table;
        b = Str2Table(value, table);
        assert(b);

        tables_.insert(std::pair<std::string, std::shared_ptr<Table>>
                       (table_name, std::make_shared<Table>(table)));
    }

    LOG(INFO) << "meta load: " << ToStringShort();

    return Status::OK();
}

Status
Meta::Persist() {
    std::vector<std::string> table_names;
    for (auto &t : tables_) {
        table_names.push_back(t.first);
    }

    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    std::string v;

    TableNames2Str(table_names, v);
    s = db_->Put(write_options, META_PERSIST_KEY_TABLES, v);
    assert(s.ok());

    for (auto &t : tables_) {
        std::string key = t.first;
        std::string value;
        Table &table = *(t.second);
        Table2Str(table, value);
        s = db_->Put(write_options, key, value);
        assert(s.ok());
    }

    return Status::OK();
}

Status
Meta::Init() {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, path_, &db_);
    assert(status.ok());
    auto s = Load();
    assert(s.ok());
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

std::shared_ptr<Table>
Meta::GetTable(const std::string &name) const {
    std::shared_ptr<Table> p;
    auto it = tables_.find(name);
    if (it != tables_.end()) {
        p = it->second;
    }
    return p;
}

std::shared_ptr<Partition>
Meta::GetPartition(const std::string &name) const {
    std::shared_ptr<Table> pt;
    std::shared_ptr<Partition> pp;
    std::string table_name;
    int partition_id;

    bool b = util::ParsePartitionName(name, table_name, partition_id);
    if (b) {
        pt = GetTable(table_name);
        if (pt) {
            auto it = pt->partitions().find(name);
            if (it != pt->partitions().end()) {
                pp = it->second;
            }
        }
    }
    return pp;
}

std::shared_ptr<Replica>
Meta::GetReplica(const std::string &name) const {
    std::shared_ptr<Partition> pp;
    std::shared_ptr<Replica> pr;
    std::string table_name;
    int partition_id;
    int replica_id;

    bool b = util::ParseReplicaName(name, table_name, partition_id, replica_id);
    if (b) {
        std::string partition_name = util::PartitionName(table_name, partition_id);
        pp = GetPartition(partition_name);
        if (pp) {
            auto it = pp->replicas().find(name);
            if (it != pp->replicas().end()) {
                pr = it->second;
            }
        }
    }
    return pr;
}

}  // namespace vectordb
