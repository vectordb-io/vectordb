#ifndef __VECTORDB_META_H__
#define __VECTORDB_META_H__

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <leveldb/db.h>
#include "status.h"
#include "config.h"

namespace vectordb {

enum EngineType {
    kVEngineAnnoy = 0,
    kVEngineFaiss = 1,
    kGEngineEasyGraph = 10,
};

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

  private:
    std::map<std::string, std::shared_ptr<Table>> tables_;

    std::string path_;
    leveldb::DB* db_;
};

}  // namespace vectordb

#endif
