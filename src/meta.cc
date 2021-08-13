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
    LOG(INFO) << "loading meta ...";
    std::vector<std::string> table_names;
    std::string value;

    auto s = db_->Get(leveldb::ReadOptions(), META_PERSIST_KEY_TABLES, &value);
    if (s.IsNotFound()) {
        return Status::OK();
    }
    assert(s.ok());

    auto b = coding::Str2TableNames(value, table_names);
    assert(b);

    for (auto &table_name : table_names) {
        LOG(INFO) << "loading meta... table:" << table_name;
        s = db_->Get(leveldb::ReadOptions(), table_name, &value);
        assert(s.ok());

        auto table_sp = std::make_shared<Table>();
        b = coding::Str2Table(value, *table_sp);
        assert(b);
        tables_.insert(std::pair<std::string, std::shared_ptr<Table>>(table_name, table_sp));
    }

    LOG(INFO) << "meta load: \n" << ToStringPretty();
    return Status::OK();
}

Status
Meta::Persist() {
    std::unique_lock<std::mutex> guard(mutex_);

    std::vector<std::string> table_names;
    for (auto &t : tables_) {
        table_names.push_back(t.first);
    }

    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    std::string v;

    coding::TableNames2Str(table_names, v);
    s = db_->Put(write_options, META_PERSIST_KEY_TABLES, v);
    assert(s.ok());

    for (auto &t : tables_) {
        std::string key = t.first;
        std::string value;
        Table &table = *(t.second);
        coding::Table2Str(table, value);
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
    if (!status.ok()) {
        LOG(INFO) << "meta init, open db error, " << status.ToString();
        return Status::OtherError("meta init, open db error");
    }

    auto s = Load();
    if (s.ok()) {
        LOG(INFO) << "meta load success";
    } else {
        LOG(INFO) << "meta load error, " << s.ToString();
    }
    return s;
}

Status
Meta::AddTable(const TableParam &param) {
    std::unique_lock<std::mutex> guard(mutex_);

    auto it = tables_.find(param.name);
    if (it != tables_.end()) {
        std::string msg = param.name;
        msg.append(" already exist");
        return Status::OtherError(msg);
    }
    auto table = std::make_shared<Table>(param);
    tables_.insert(std::pair<std::string, std::shared_ptr<Table>>(param.name, table));

    std::string msg = "add table: ";
    msg.append(table->ToStringPretty());
    LOG(INFO) << msg;
    return Status::OK();
}

Status
Meta::DropTable(const std::string &name) {
    std::unique_lock<std::mutex> guard(mutex_);

    tables_.erase(name);
    return Status::OK();
}

Status
Meta::ReplicaNameByKey(const std::string &table_name,
                       const std::string &key, std::string &replica_name) const {
    std::unique_lock<std::mutex> guard(mutex_);

    auto it_table = tables_.find(table_name);
    if (it_table == tables_.end()) {
        std::string msg = "table not found: ";
        msg.append(table_name);
        return Status::NotFound(msg);
    }
    int partition_id = util::RSHash(key.c_str()) % it_table->second->partition_num();
    replica_name = util::ReplicaName(table_name, partition_id, 0);
    return Status::OK();
}

Status
Meta::ReplicaNamesByTable(const std::string &table_name,
                          std::vector<std::string> &replica_names) const {
    std::unique_lock<std::mutex> guard(mutex_);

    auto table_sp = GetTableNonlocking(table_name);
    if (!table_sp) {
        std::string msg = "table not found: ";
        msg.append(table_name);
        return Status::NotFound(msg);
    }

    replica_names.clear();
    for (auto &partition_kv : table_sp->partitions()) {
        auto partition_sp = partition_kv.second;
        for (auto &replica_kv : partition_sp->replicas()) {
            replica_names.push_back(replica_kv.first);
            //LOG(INFO) << "ReplicaNamesByTable get replica: " << replica_kv.first;
        }
    }

    return Status::OK();
}

std::shared_ptr<Table>
Meta::GetTable(const std::string &name) const {
    std::unique_lock<std::mutex> guard(mutex_);

    std::shared_ptr<Table> p;
    auto it = tables_.find(name);
    if (it != tables_.end()) {
        p = it->second;
    }
    return p;
}

std::shared_ptr<Table>
Meta::GetTableNonlocking(const std::string &name) const {
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
        pt = GetTable(table_name);  // lock
        if (pt) {
            pp = pt->GetPartition(name);
        }
    }
    return pp;
}

std::shared_ptr<Partition>
Meta::GetPartitionNonlocking(const std::string &name) const {
    std::shared_ptr<Table> pt;
    std::shared_ptr<Partition> pp;
    std::string table_name;
    int partition_id;

    bool b = util::ParsePartitionName(name, table_name, partition_id);
    if (b) {
        pt = GetTableNonlocking(table_name);
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
        pp = GetPartition(partition_name); // lock
        if (pp) {
            pr = pp->GetReplica(name);
        }
    }
    return pr;
}

Status
Meta::ForEachTable(std::function<Status(std::shared_ptr<Table>)> func) {
    std::unique_lock<std::mutex> guard(mutex_);

    for (auto &table_kv : tables_) {
        auto s = func(table_kv.second);
        if (!s.ok()) {
            std::string msg = "ForEachTable do ";
            msg.append(table_kv.first).append(" error");
            LOG(INFO) << msg;
            return s;
        }
    }
    return Status::OK();
}

Status
Meta::ForEachPartition(std::function<Status(std::shared_ptr<Partition>)> func) {
    std::unique_lock<std::mutex> guard(mutex_);

    for (auto &table_kv : tables_) {
        for (auto &partition_kv : table_kv.second->partitions()) {
            auto partition_sp = partition_kv.second;
            auto s = func(partition_sp);
            if (!s.ok()) {
                std::string msg = "ForEachPartition ";
                msg.append(partition_kv.first).append(" error");
                LOG(INFO) << msg;
                return s;
            }
        }
    }
    return Status::OK();
}

Status
Meta::ForEachReplica(std::function<Status(std::shared_ptr<Replica>)> func) {
    std::unique_lock<std::mutex> guard(mutex_);

    for (auto &table_kv : tables_) {
        for (auto &partition_kv : table_kv.second->partitions()) {
            for (auto &replica_kv : partition_kv.second->replicas()) {
                auto replica_sp = replica_kv.second;
                auto s = func(replica_sp);
                if (!s.ok()) {
                    std::string msg = "ForEachReplica ";
                    msg.append(replica_kv.first).append(" error");
                    LOG(INFO) << msg;
                    return s;
                }
            }
        }
    }
    return Status::OK();
}

Status
Meta::ForEachReplica2(std::function<Status(std::shared_ptr<Table>, std::shared_ptr<Partition>, std::shared_ptr<Replica>)> func) {
    std::unique_lock<std::mutex> guard(mutex_);

    for (auto &table_kv : tables_) {
        for (auto &partition_kv : table_kv.second->partitions()) {
            for (auto &replica_kv : partition_kv.second->replicas()) {
                auto replica_sp = replica_kv.second;
                auto s = func(table_kv.second, partition_kv.second, replica_sp);
                if (!s.ok()) {
                    std::string msg = "ForEachReplica ";
                    msg.append(replica_kv.first).append(" error");
                    LOG(INFO) << msg;
                    return s;
                }
            }
        }
    }
    return Status::OK();
}

Status
Meta::ForEachReplicaOfTable(const std::string &table_name, std::function<Status(std::shared_ptr<Table>, std::shared_ptr<Partition>, std::shared_ptr<Replica>)> func) {
    std::unique_lock<std::mutex> guard(mutex_);

    std::shared_ptr<Table> table_sp;
    auto it = tables_.find(table_name);
    if (it != tables_.end()) {
        table_sp = it->second;
    } else {
        std::string msg = "table not found: ";
        msg.append(table_name);
        return Status::NotFound(table_name);
    }

    for (auto &partition_kv : table_sp->partitions()) {
        auto partition_sp = partition_kv.second;
        for (auto &replica_kv : partition_sp->replicas()) {
            auto replica_sp = replica_kv.second;
            auto s = func(table_sp, partition_sp, replica_sp);
            if (!s.ok()) {
                std::string msg = "ForEachReplicaOfTable ";
                msg.append(replica_kv.first).append(" error");
                LOG(INFO) << msg;
                return s;
            }
        }
    }

    return Status::OK();
}

jsonxx::json64
Replica::ToJson() const {
    jsonxx::json64 j, jret;
    j["id"] = id_;
    j["name"] = name_;
    j["table_name"] = table_name_;
    j["partition_name"] = partition_name_;
    j["address"] = address_;
    j["path"] = path_;

    jret["Replica"] = j;
    return jret;
}

jsonxx::json64
Partition::ToJson() const {
    jsonxx::json64 j, jret;
    j["id"] = id_;
    j["name"] = name_;
    j["table_name"] = table_name_;
    j["replica_num"] = replica_num_;
    j["path"] = path_;
    int k = 0;
    for (auto &r : replicas_) {
        j["replicas"][k++] = r.second->name();
    }

    jret["Partition"] = j;
    return jret;
}

jsonxx::json64
Table::ToJson() const {
    jsonxx::json64 j, jret;
    j["name"] = name_;
    j["dim"] = dim_;
    j["partition_num"] = partition_num_;
    j["replica_num"] = replica_num_;
    j["path"] = path_;
    int k = 0;
    for (auto &p : partitions_) {
        j["partitions"][k] = p.second->name();
        k++;
    }

    {
        std::unique_lock<std::mutex> guard(mutex_);
        int i = 0;
        for (auto &index_name : indices_) {
            j["indices"][i] = index_name;
            i++;
        }
    }

    jret["Table"] = j;
    return jret;
}

jsonxx::json64
Meta::ToJson() const {
    std::unique_lock<std::mutex> guard(mutex_);

    jsonxx::json64 j, jret;
    for (auto &kv : tables_) {
        j[kv.first] = kv.second->ToJson();
    }
    jret["Meta"] = j;
    return jret;
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

        std::string msg = "add replica: ";
        msg.append(replica->ToStringPretty());
        LOG(INFO) << msg;
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

        std::string msg = "add partition: ";
        msg.append(partition->ToStringPretty());
        LOG(INFO) << msg;
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
