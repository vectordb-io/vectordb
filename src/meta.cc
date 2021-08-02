#include <glog/logging.h>
#include "util.h"
#include "coding.h"
#include "meta.h"

namespace vectordb {

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
Meta::AddTable(const TableParam &param) {
    auto it = tables_.find(param.name);
    if (it != tables_.end()) {
        std::string msg = param.name;
        msg.append(" already exist");
        return Status::Corruption(msg);
    }
    auto table = std::make_shared<Table>(param);
    tables_.insert(std::pair<std::string, std::shared_ptr<Table>>(param.name, table));
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
    int partition_id = util::RSHash(key.c_str()) % it_table->second->partition_num();
    replica_name = util::ReplicaName(table_name, partition_id, 0);
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
    if (b) {
        std::string partition_name = util::PartitionName(table_name, partition_id);
        pp = GetPartition(partition_name);
        if (pp) {
            pr = pp->GetReplica(name);
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
    j["dim"] = dim_;
    j["partition_num"] = partition_num_;
    j["replica_num"] = replica_num_;
    j["engine_type"] = engine_type_;
    j["path"] = path_;
    int k = 0;
    for (auto &p : partitions_) {
        j["partitions"][k++] = p.second->name();
    }

    k = 0;
    for (auto &kv : indices_) {
        jsonxx::json ji;
        ji["index_name"] = kv.first;
        ji["index_type"] = kv.second;
        j["indices"][k++] = ji;
    }
    return j;
}

std::string
Meta::ToString() const {
    std::string s;
    s.append("meta:\n");
    for (auto &t : tables_) {
        s.append((t.second)->ToString());
        s.append("\n");
    }
    return s;
}

std::string
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
Partition::AddAllReplicas() {
    assert(replica_num_ > 0);
    for (int replica_id = 0; replica_id < replica_num_; ++replica_id) {
        std::string replica_name = util::ReplicaName(table_name_, id_, replica_id);
        char buf[256];
        snprintf(buf, sizeof(buf), "%s/%d", path_.c_str(), replica_id);
        std::string replica_path = std::string(buf);

        ReplicaParam param;
        param.id = replica_id;
        param.name = replica_name;
        param.table_name = table_name_;
        param.partition_name = name_;
        param.path = replica_path;
        auto replica = std::make_shared<Replica>(param);
        replicas_.insert(std::pair<std::string, std::shared_ptr<Replica>>(replica_name, replica));
    }
}

void
Table::AddAllPartitions() {
    assert(partition_num_ > 0);
    for (int partition_id = 0; partition_id < partition_num_; ++partition_id) {
        std::string partition_name = util::PartitionName(name_, partition_id);
        char buf[256];
        snprintf(buf, sizeof(buf), "%s/%d", path_.c_str(), partition_id);
        std::string partition_path = std::string(buf);

        PartitionParam param;
        param.id = partition_id;
        param.name = partition_name;
        param.table_name = name_;
        param.replica_num = replica_num_;
        param.path = partition_path;
        auto partition = std::make_shared<Partition>(param);
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

Status
Meta::AddIndex(const IndexParam &param) {
    auto it_table = tables_.find(param.table_name);
    if (it_table == tables_.end()) {
        std::string msg = param.table_name;
        msg.append(" not exist");
        return Status::Corruption(msg);
    }
    assert(param.index_type == VECTOR_INDEX_ANNOY || param.index_type == VECTOR_INDEX_KNNGRAPH);

    auto it_index = it_table->second->indices().find(param.index_name);
    if (it_index != it_table->second->indices().end()) {
        std::string msg = param.index_name;
        msg.append(" already exist");
        return Status::Corruption(msg);
    }

    it_table->second->mutable_indices().insert(std::pair<std::string, std::string>(param.index_name, param.index_type));
    return Status::OK();
}

} // namespace vectordb
