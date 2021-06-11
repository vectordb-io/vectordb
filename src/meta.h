#ifndef __VECTORDB_META_H__
#define __VECTORDB_META_H__

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <leveldb/db.h>
#include "status.h"
#include "config.h"
#include "util.h"

namespace vectordb {

#define META_PERSIST_KEY_TABLES "META_PERSIST_KEY_TABLES"

enum EngineType {
    kVEngineAnnoy = 0,
    kVEngineFaiss = 1,
    kGEngineEasyGraph = 10,
};

std::string EngineTypeToString(EngineType e);

class Replica {
  public:
    Replica() {
    }

    Replica(const Replica&) = default;

    Replica(int id,
            const std::string &name,
            const std::string &table_name,
            const std::string &partition_name,
            EngineType engine_type,
            const std::string &path)
        :id_(id),
         name_(name),
         table_name_(table_name),
         partition_name_(partition_name),
         engine_type_(engine_type),
         path_(path) {
        address_ = Config::GetInstance().address().ToString();
    }

    void Init(int id,
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

    int id() const {
        return id_;
    }

    const std::string&
    name() const {
        return name_;
    }

    const std::string&
    table_name() const {
        return table_name_;
    }

    const std::string&
    partition_name() const {
        return partition_name_;
    }

    EngineType engine_type() const {
        return engine_type_;
    }

    const std::string&
    address() const {
        return address_;
    }

    const std::string&
    path() const {
        return path_;
    }

    const std::string
    ToStringShort() const {
        char buf[256];
        std::string s;
        s.append("replica:{");
        snprintf(buf, sizeof(buf), "name:%s", name_.c_str());
        s.append(buf);
        s.append("}");
        return s;
    }

    const std::string
    ToString() const {
        char buf[256];
        std::string s;

        s.append("replica:{");
        snprintf(buf, sizeof(buf), "id:%d, ", id_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "name:%s, ", name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "table_name:%s, ", table_name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "partition_name:%s, ", partition_name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "engine_type:%s, ", EngineTypeToString(engine_type_).c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "address:%s, ", address_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "path:%s", path_.c_str());
        s.append(buf);
        s.append("}");
        return s;
    }

    const std::string
    ToStringInfo() const {
        char buf[256];
        std::string s;

        s.append("replica:{\n");
        snprintf(buf, sizeof(buf), "id:%d \n", id_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "name:%s \n", name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "table_name:%s \n", table_name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "partition_name:%s \n", partition_name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "engine_type:%s \n", EngineTypeToString(engine_type_).c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "address:%s \n", address_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "path:%s \n", path_.c_str());
        s.append(buf);
        s.append("}\n");
        return s;
    }

  private:
    int id_;
    std::string name_;
    std::string table_name_;
    std::string partition_name_;
    EngineType engine_type_;
    std::string address_;
    std::string path_;
};

class Partition {
  public:
    Partition() {
    }

    Partition(int id,
              const std::string &name,
              const std::string &table_name,
              int replica_num,
              EngineType engine_type,
              const std::string &path)
        :id_(id),
         name_(name),
         table_name_(table_name),
         replica_num_(replica_num),
         engine_type_(engine_type),
         path_(path) {
        AddReplicas();
    }

    void Init(int id,
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

    int id() const {
        return id_;
    }

    const std::string&
    name() const {
        return name_;
    }

    const std::string&
    table_name() const {
        return table_name_;
    }

    int replica_num() const {
        return replica_num_;
    }

    EngineType engine_type() const {
        return engine_type_;
    }

    const std::string&
    path() const {
        return path_;
    }

    const std::map<std::string, std::shared_ptr<Replica>>&
    replicas() const {
        return replicas_;
    }

    void AddReplica(const Replica &r) {
        auto it = replicas_.find(r.name());
        assert(it == replicas_.end());
        auto replica_sp = std::make_shared<Replica>(r);
        replicas_.insert(std::pair<std::string, std::shared_ptr<Replica>>(replica_sp->name(), replica_sp));
    }

    const std::string
    ToStringShort() const {
        char buf[256];
        std::string s;
        s.append("partition:{");
        snprintf(buf, sizeof(buf), "name:%s, ", name_.c_str());
        s.append(buf);
        s.append("replicas:{");
        for (auto &r : replicas_) {
            s.append((r.second)->ToStringShort());
            s.append(", ");
        }
        s.pop_back();
        s.pop_back();
        s.append("}");
        s.append("}");
        return s;
    }

    const std::string
    ToString() const {
        char buf[256];
        std::string s;

        s.append("partition:{");
        snprintf(buf, sizeof(buf), "id:%d, ", id_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "name:%s, ", name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "table_name:%s, ", table_name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "replica_num:%d, ", replica_num_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "engine_type:%s, ", EngineTypeToString(engine_type_).c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "path:%s, ", path_.c_str());
        s.append(buf);
        s.append("replicas:{");
        for (auto &r : replicas_) {
            s.append((r.second)->ToString());
            s.append(", ");
        }
        s.pop_back();
        s.pop_back();
        s.append("}");
        s.append("}");
        return s;
    }

    const std::string
    ToStringInfo() const {
        char buf[256];
        std::string s;

        s.append("partition:{\n");
        snprintf(buf, sizeof(buf), "id:%d \n", id_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "name:%s \n", name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "table_name:%s \n", table_name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "replica_num:%d \n", replica_num_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "engine_type:%s \n", EngineTypeToString(engine_type_).c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "path:%s \n", path_.c_str());
        s.append(buf);
        s.append("replicas:{");
        for (auto &r : replicas_) {
            s.append((r.second)->name());
            s.append(", ");
        }
        s.pop_back();
        s.pop_back();
        s.append("}\n");
        s.append("}\n");
        return s;
    }

  private:
    void AddReplicas() {
        assert(replica_num_ > 0);
        for (int replica_id = 0; replica_id < replica_num_; ++replica_id) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s#partition%d#replica%d", table_name_.c_str(), id_, replica_id);
            std::string replica_name = std::string(buf);
            snprintf(buf, sizeof(buf), "%s/%d", path_.c_str(), replica_id);
            std::string replica_path = std::string(buf);
            auto replica = std::make_shared<Replica>(replica_id, replica_name, table_name_, name_, engine_type_, replica_path);
            replicas_.insert(std::pair<std::string, std::shared_ptr<Replica>>(replica_name, replica));
        }
    }

    int id_;
    std::string name_;
    std::string table_name_;
    int replica_num_;
    EngineType engine_type_;
    std::string path_;
    std::map<std::string, std::shared_ptr<Replica>> replicas_;
};

class Table {
  public:
    Table() {
    }

    Table(const std::string& name,
          int partition_num,
          int replica_num,
          EngineType engine_type,
          const std::string& path)
        :name_(name),
         partition_num_(partition_num),
         replica_num_(replica_num),
         engine_type_(engine_type),
         path_(path) {
        AddPartitions();
    }

    Table(const Table&) = default;

    void Init(const std::string& name,
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

    const std::string&
    name() const {
        return name_;
    }

    int partition_num() const {
        return partition_num_;
    }

    int replica_num() const {
        return replica_num_;
    }

    EngineType engine_type() const {
        return engine_type_;
    }

    const std::string&
    path() const {
        return path_;
    }

    const std::map<std::string, std::shared_ptr<Partition>>&
    partitions() const {
        return partitions_;
    }

    void AddPartition(const Partition &p) {
        auto it = partitions_.find(p.name());
        assert(it == partitions_.end());
        auto partition_sp = std::make_shared<Partition>(p);
        partitions_.insert(std::pair<std::string, std::shared_ptr<Partition>>(partition_sp->name(), partition_sp));
    }

    const std::string
    ToStringShort() const {
        char buf[256];
        std::string s;
        s.append("table:{");
        snprintf(buf, sizeof(buf), "name:%s, ", name_.c_str());
        s.append(buf);
        s.append("partitions:{");
        for (auto &p : partitions_) {
            s.append((p.second)->ToStringShort());
            s.append(", ");
        }
        s.pop_back();
        s.pop_back();
        s.append("}");
        s.append("}");
        return s;
    }

    const std::string
    ToString() const {
        char buf[256];
        std::string s;

        s.append("table:{");
        snprintf(buf, sizeof(buf), "name:%s, ", name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "partition_num:%d, ", partition_num_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "replica_num:%d, ", replica_num_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "engine_type:%s, ", EngineTypeToString(engine_type_).c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "path:%s, ", path_.c_str());
        s.append(buf);
        s.append("partitions:{");
        for (auto &p : partitions_) {
            s.append((p.second)->ToString());
            s.append(", ");
        }
        s.pop_back();
        s.pop_back();
        s.append("}");
        s.append("}");
        return s;
    }

    const std::string
    ToStringInfo() const {
        char buf[256];
        std::string s;

        s.append("table:{\n");
        snprintf(buf, sizeof(buf), "name:%s \n", name_.c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "partition_num:%d \n", partition_num_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "replica_num:%d \n", replica_num_);
        s.append(buf);
        snprintf(buf, sizeof(buf), "engine_type:%s \n", EngineTypeToString(engine_type_).c_str());
        s.append(buf);
        snprintf(buf, sizeof(buf), "path:%s \n", path_.c_str());
        s.append(buf);
        s.append("partitions:{\n");
        for (auto &p : partitions_) {
            s.append((p.second)->ToStringInfo());
            s.append("\n");
        }
        s.pop_back();
        s.pop_back();
        s.append("}\n");
        s.append("}\n");
        return s;
    }

  private:
    void AddPartitions() {
        assert(partition_num_ > 0);
        for (int partition_id = 0; partition_id < partition_num_; ++partition_id) {
            char buf[256];
            snprintf(buf, sizeof(buf), "%s#partition%d", name_.c_str(), partition_id);
            std::string partition_name = std::string(buf);
            snprintf(buf, sizeof(buf), "%s/%d", path_.c_str(), partition_id);
            std::string partition_path = std::string(buf);
            auto partition = std::make_shared<Partition>(partition_id, partition_name, name_, replica_num_, engine_type_, partition_path);
            partitions_.insert(std::pair<std::string, std::shared_ptr<Partition>>(partition_name, partition));
        }
    }

    std::string name_;
    int partition_num_;
    int replica_num_;
    EngineType engine_type_;
    std::string path_;
    std::map<std::string, std::shared_ptr<Partition>> partitions_;
};


class Meta {
  public:
    Meta(const std::string &path);
    ~Meta() = default;
    Meta(const Meta&) = delete;
    Meta& operator=(const Meta&) = delete;

    Status Init();
    Status Load();
    Status Persist();

    Status AddTable(const std::string &name,
                    int partition_num,
                    int replica_num,
                    EngineType engine_type,
                    const std::string &path);

    Status DropTable(const std::string &name);

    Status ReplicaName(const std::string &table_name,
                       const std::string &key,
                       std::string &replica_name) const;

    const std::string
    ToString() const {
        std::string s;
        s.append("meta:{\n");
        for (auto &t : tables_) {
            s.append((t.second)->ToString());
            s.append("\n");
        }
        s.append("}\n");
        return s;
    }

    const std::string
    ToStringInfo() const {
        std::string s;
        s.append("meta:{\n");
        for (auto &t : tables_) {
            s.append((t.second)->ToStringInfo());
            s.append("\n");
        }
        s.append("}\n");
        return s;
    }

    const std::string
    ToStringShort() const {
        std::string s;
        s.append("meta:{\n");
        for (auto &t : tables_) {
            s.append(t.first);
            s.append(":\n");
            for (auto &p : (t.second)->partitions()) {
                for (auto &r : (p.second)->replicas()) {
                    s.append((r.second)->ToStringShort());
                    s.append("\n");
                }
            }
            s.append("\n");
        }
        s.pop_back();
        s.append("}\n");
        return s;
    }

  private:
    std::map<std::string, std::shared_ptr<Table>> tables_;

    std::string path_;
    leveldb::DB* db_;
};

}  // namespace vectordb

#endif
