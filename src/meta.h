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

#define KV_ENGINE "kv"
#define VECTOR_ENGINE "vector"
#define GRAPH_ENGINE "graph"

#define VECTOR_INDEX_ANNOY "annoy"
#define VECTOR_INDEX_KNNGRAPH "knn_graph"

struct IndexParam {
    std::string table_name;
    std::string index_name;
    std::string index_type;
};

struct ReplicaParam {
    int id;
    std::string name;
    std::string table_name;
    std::string partition_name;
    std::string path;
};

struct PartitionParam {
    int id;
    std::string name;
    std::string table_name;
    int replica_num;
    std::string path;
};

struct TableParam {
    std::string name;
    int partition_num;
    int replica_num;
    std::string engine_type;
    std::string path;
    int dim;
};

class Replica {
  public:
    Replica() = default;
    Replica(const ReplicaParam &param)
        :id_(param.id),
         name_(param.name),
         table_name_(param.table_name),
         partition_name_(param.partition_name),
         path_(param.path) {
        address_ = Config::GetInstance().address().ToString();
    }
    Replica(const Replica&) = default;
    Replica& operator=(const Replica&) = default;
    ~Replica() = default;

    int id() const {
        return id_;
    }

    void set_id(int id) {
        id_ = id;
    }

    std::string name() const {
        return name_;
    }

    void set_name(const std::string& name) {
        name_ = name;
    }

    std::string table_name() const {
        return table_name_;
    }

    void set_table_name(const std::string& table_name) {
        table_name_ = table_name;
    }

    std::string partition_name() const {
        return partition_name_;
    }

    void set_partition_name(const std::string& partition_name) {
        partition_name_ = partition_name;
    }

    std::string address() const {
        return address_;
    }

    void set_address(const std::string& address) {
        address_ = address;
    }

    std::string path() const {
        return path_;
    }

    void set_path(const std::string& path) {
        path_ = path;
    }

    std::string ToString() const {
        return ToJson().dump();
    }

    std::string ToStringPretty() const {
        return ToJson().dump(4, ' ');
    }

  private:
    jsonxx::json ToJson() const;

    int id_;
    std::string name_;
    std::string table_name_;
    std::string partition_name_;
    std::string address_;
    std::string path_;
};

class Partition {
  public:
    Partition() = default;
    Partition(const PartitionParam& param)
        :id_(param.id),
         name_(param.name),
         table_name_(param.table_name),
         replica_num_(param.replica_num),
         path_(param.path) {
        AddAllReplicas();
    }
    Partition(const Partition&) = default;
    Partition& operator=(const Partition&) = default;
    ~Partition() = default;

    std::shared_ptr<Replica>
    GetReplica(std::string name) const;

    int id() const {
        return id_;
    }

    void set_id(int id) {
        id_ = id;
    }

    std::string name() const {
        return name_;
    }

    void set_name(const std::string& name) {
        name_ = name;
    }

    std::string table_name() const {
        return table_name_;
    }

    void set_table_name(const std::string& table_name) {
        table_name_ = table_name;
    }

    int replica_num() const {
        return replica_num_;
    }

    void set_replica_num(int replica_num) {
        replica_num_ = replica_num;
    }

    std::string path() const {
        return path_;
    }

    void set_path(const std::string& path) {
        path_ = path;
    }

    std::map<std::string, std::shared_ptr<Replica>>&
    mutable_replicas() {
        return replicas_;
    }

    const std::map<std::string, std::shared_ptr<Replica>>&
    replicas() const {
        return replicas_;
    }

    std::string ToString() const {
        return ToJson().dump();
    }

    std::string ToStringPretty() const {
        return ToJson().dump(4, ' ');
    }

  private:
    void AddAllReplicas();
    jsonxx::json ToJson() const;

    int id_;
    std::string name_;
    std::string table_name_;
    int replica_num_;
    std::string path_;
    std::map<std::string, std::shared_ptr<Replica>> replicas_;
};

class Table {
  public:
    Table() = default;
    Table(const TableParam &param)
        :name_(param.name),
         dim_(param.dim),
         partition_num_(param.partition_num),
         replica_num_(param.replica_num),
         engine_type_(param.engine_type),
         path_(param.path) {
        AddAllPartitions();
    }
    Table(const Table&) = default;
    Table& operator=(const Table&) = default;
    ~Table() = default;

    std::shared_ptr<Partition>
    GetPartition(std::string name) const;

    std::string name() const {
        return name_;
    }

    void set_name(const std::string& name) {
        name_ = name;
    }

    int partition_num() const {
        return partition_num_;
    }

    int dim() const {
        return dim_;
    }

    void set_dim(int dim) {
        dim_ = dim;
    }

    void set_partition_num(int partition_num) {
        partition_num_ = partition_num;
    }

    int replica_num() const {
        return replica_num_;
    }

    void set_replica_num(int replica_num) {
        replica_num_ = replica_num;
    }

    std::string engine_type() const {
        return engine_type_;
    }

    void set_engine_type(const std::string& engine_type) {
        engine_type_ = engine_type;
    }

    std::string path() const {
        return path_;
    }

    void set_path(const std::string& path) {
        path_ = path;
    }

    std::map<std::string, std::shared_ptr<Partition>>&
    mutable_partitions() {
        return partitions_;
    }

    const std::map<std::string, std::shared_ptr<Partition>>&
    partitions() const {
        return partitions_;
    }

    std::map<std::string, std::string>&
    mutable_indices() {
        return indices_;
    }

    const std::map<std::string, std::string>&
    indices() const {
        return indices_;
    }

    std::string ToString() const {
        return ToJson().dump();
    }

    std::string ToStringPretty() const {
        return ToJson().dump(4, ' ');
    }

  private:
    void AddAllPartitions();
    jsonxx::json ToJson() const;

    std::string name_;
    int dim_;
    int partition_num_;
    int replica_num_;
    std::string engine_type_;
    std::string path_;
    std::map<std::string, std::shared_ptr<Partition>> partitions_;
    std::map<std::string, std::string> indices_;
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
    Status AddTable(const TableParam &param);
    Status AddIndex(const IndexParam &param);
    Status DropTable(const std::string &name);
    Status ReplicaName(const std::string &table_name,
                       const std::string &key,
                       std::string &replica_name) const;
    std::string ToString() const;
    std::string ToStringPretty() const;

    std::shared_ptr<Table>
    GetTable(const std::string &name) const;

    std::shared_ptr<Partition>
    GetPartition(const std::string &name) const;

    std::shared_ptr<Replica>
    GetReplica(const std::string &name) const;

    std::map<std::string, std::shared_ptr<Table>>&
    mutable_tables() {
        return tables_;
    }

    const std::map<std::string, std::shared_ptr<Table>>&
    tables() const {
        return tables_;
    }

  private:
    std::map<std::string, std::shared_ptr<Table>> tables_;

    std::string path_;
    leveldb::DB* db_;
};

} // namespace vectordb

#endif
