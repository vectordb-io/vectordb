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

    LOG(INFO) << "meta load: \n" << ToStringPretty();

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
            pp = pt->GetPartition(name);
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

    LOG(INFO) << "debug b: " << b;

    if (b) {
        std::string partition_name = util::PartitionName(table_name, partition_id);
        pp = GetPartition(partition_name);

        LOG(INFO) << "debug partition_name: " << partition_name;

        if (pp) {
            pr = pp->GetReplica(name);

            LOG(INFO) << "debug GetReplica : " << name;

            if (pr) {
                LOG(INFO) << "debug get replcia: " << pr->ToStringPretty();
            }
        }
    }
    return pr;
}

jsonxx::json
Replica::ToJson() const {
    jsonxx::json j;
    j["id"] = id_;
    j["name"] = name_;
    j["table_name"] = table_name_;
    j["partition_name"] = partition_name_;
    j["engine_type"] = engine_type_;
    j["address"] = address_;
    j["path"] = path_;
    return j;
}

jsonxx::json
Partition::ToJson() const {
    jsonxx::json j;
    j["id"] = id_;
    j["name"] = name_;
    j["table_name"] = table_name_;
    j["replica_num"] = replica_num_;
    j["engine_type"] = engine_type_;
    j["path"] = path_;
    int k = 0;
    for (auto &r : replicas_) {
        j["replicas"][k++] = r.second->name();
    }
    return j;
}

jsonxx::json
Table::ToJson() const {
    jsonxx::json j;
    j["name"] = name_;
    j["partition_num"] = partition_num_;
    j["replica_num"] = replica_num_;
    j["engine_type"] = engine_type_;
    j["path"] = path_;
    int k = 0;
    for (auto &p : partitions_) {
        j["partitions"][k++] = p.second->name();
    }
    return j;
}

const std::string
Meta::ToString() const {
    std::string s;
    s.append("meta:\n");
    for (auto &t : tables_) {
        s.append((t.second)->ToString());
        s.append("\n");
    }
    return s;
}

const std::string
Meta::ToStringPretty() const {
    std::string s;
    s.append("meta:\n");
    for (auto &t : tables_) {
        s.append((t.second)->ToStringPretty());
        s.append("\n");
    }
    return s;
}

void
Partition::AddReplica(const Replica &r) {
    auto it = replicas_.find(r.name());
    assert(it == replicas_.end());
    auto replica_sp = std::make_shared<Replica>(r);
    replicas_.insert(std::pair<std::string, std::shared_ptr<Replica>>(replica_sp->name(), replica_sp));
}

void
Partition::AddReplicas() {
    assert(replica_num_ > 0);
    for (int replica_id = 0; replica_id < replica_num_; ++replica_id) {
        std::string replica_name = util::ReplicaName(table_name_, id_, replica_id);
        char buf[256];
        snprintf(buf, sizeof(buf), "%s/%d", path_.c_str(), replica_id);
        std::string replica_path = std::string(buf);
        auto replica = std::make_shared<Replica>(replica_id, replica_name, table_name_, name_, engine_type_, replica_path);
        replicas_.insert(std::pair<std::string, std::shared_ptr<Replica>>(replica_name, replica));
    }
}

void
Replica::Init(int id,
              const std::string &name,
              const std::string &table_name,
              const std::string &partition_name,
              EngineType engine_type,
              const std::string &path) {
    id_ = id;
    name_ = name;
    table_name_ = table_name;
    partition_name_ = partition_name;
    engine_type_ = engine_type;
    path_ = path;
    address_ = Config::GetInstance().address().ToString();
}

void
Partition::Init(int id,
                const std::string &name,
                const std::string &table_name,
                int replica_num,
                EngineType engine_type,
                const std::string &path) {
    id_ = id;
    name_ = name;
    table_name_ = table_name;
    replica_num_ = replica_num;
    engine_type_ = engine_type;
    path_ = path;
}

void
Table::Init(const std::string& name,
            int partition_num,
            int replica_num,
            EngineType engine_type,
            const std::string& path) {
    name_ = name;
    partition_num_ = partition_num;
    replica_num_ = replica_num;
    engine_type_ = engine_type;
    path_ = path;
}

void
Table::AddPartition(const Partition &p) {
    auto it = partitions_.find(p.name());
    assert(it == partitions_.end());
    auto partition_sp = std::make_shared<Partition>(p);
    partitions_.insert(std::pair<std::string, std::shared_ptr<Partition>>(partition_sp->name(), partition_sp));
}

void
Table::AddPartitions() {
    assert(partition_num_ > 0);
    for (int partition_id = 0; partition_id < partition_num_; ++partition_id) {
        std::string partition_name = util::PartitionName(name_, partition_id);
        char buf[256];
        snprintf(buf, sizeof(buf), "%s/%d", path_.c_str(), partition_id);
        std::string partition_path = std::string(buf);
        auto partition = std::make_shared<Partition>(partition_id, partition_name, name_, replica_num_, engine_type_, partition_path);
        partitions_.insert(std::pair<std::string, std::shared_ptr<Partition>>(partition_name, partition));
    }
}

std::shared_ptr<Replica>
Partition::GetReplica(std::string name) const {
    std::shared_ptr<Replica> pr;
    auto it = replicas_.find(name);
    if (it != replicas_.end()) {
        pr = it->second;
    }
    return pr;
}

std::shared_ptr<Partition>
Table::GetPartition(std::string name) const {
    std::shared_ptr<Partition> pp;
    auto it = partitions_.find(name);
    if (it != partitions_.end()) {
        pp = it->second;
    }
    return pp;
}


} // namespace vectordb
