#ifndef __VECTORDB_META_H__
#define __VECTORDB_META_H__

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <leveldb/db.h>
#include "jsonxx/json.hpp"
#include "status.h"
#include "config.h"
#include "util.h"

namespace vectordb {

#define META_PERSIST_KEY_TABLES "META_PERSIST_KEY_TABLES"

enum EngineType {
    kKVEngine = 0,
    kVectorEngine = 10,
    kGraphEngine = 20,
    kErrorEngine = 100,
};

std::string EngineTypeToString(EngineType e);
EngineType StringToEngineType(const std::string &s);


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
              const std::string &path);

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
    ToString() const {
        return ToJson().dump();
    }

    const std::string
    ToStringPretty() const {
        return ToJson().dump(4, ' ');
    }

  private:
    jsonxx::json ToJson() const;

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
              const std::string &path);
    void AddReplica(const Replica &r);
    void AddReplicas();

    std::shared_ptr<Replica>
    GetReplica(std::string name) const;

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

    const std::string
    ToString() const {
        return ToJson().dump();
    }

    const std::string
    ToStringPretty() const {
        return ToJson().dump(4, ' ');
    }


  private:
    jsonxx::json ToJson() const;

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
              const std::string& path);
    void AddPartition(const Partition &p);
    void AddPartitions();

    std::shared_ptr<Partition>
    GetPartition(std::string name) const;

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

    const std::string
    ToString() const {
        return ToJson().dump();
    }

    const std::string
    ToStringPretty() const {
        return ToJson().dump(4, ' ');
    }

  private:
    jsonxx::json ToJson() const;

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

    const std::string ToString() const;
    const std::string ToStringPretty() const;

    std::map<std::string, std::shared_ptr<Table>>&
    tables() {
        return tables_;
    }

    std::shared_ptr<Table>
    GetTable(const std::string &name) const;

    std::shared_ptr<Partition>
    GetPartition(const std::string &name) const;

    std::shared_ptr<Replica>
    GetReplica(const std::string &name) const;

  private:
    std::map<std::string, std::shared_ptr<Table>> tables_;

    std::string path_;
    leveldb::DB* db_;
};

} // namespace vectordb

#endif
