#ifndef __VECTORDB_META_H__
#define __VECTORDB_META_H__

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <functional>
#include <leveldb/db.h>
#include "jsonxx/json.hpp"
#include "status.h"
#include "config.h"
#include "util.h"

namespace vectordb {

struct ReplicaParam {
    int id;
    std::string name;
    std::string table_name;
    std::string partition_name;
    std::string path;
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

    jsonxx::json64 ToJson() const;

  private:

    int id_;
    std::string name_;
    std::string table_name_;
    std::string partition_name_;
    std::string address_;
    std::string path_;
};


struct PartitionParam {
    int id;
    std::string name;
    std::string table_name;
    int replica_num;
    std::string path;
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

    jsonxx::json64 ToJson() const;

  private:
    void AddAllReplicas();

    int id_;
    std::string name_;
    std::string table_name_;
    int replica_num_;
    std::string path_;
    std::map<std::string, std::shared_ptr<Replica>> replicas_;
};


struct TableParam {
    std::string name;
    int partition_num;
    int replica_num;
    std::string path;
    int dim;
};

class Table {
  public:
    Table() = default;
    Table(const TableParam &param)
        :name_(param.name),
         dim_(param.dim),
         partition_num_(param.partition_num),
         replica_num_(param.replica_num),
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

    jsonxx::json64 ToJson() const;

  private:
    void AddAllPartitions();

    std::string name_;
    int dim_;
    int partition_num_;
    int replica_num_;
    std::string path_;
    std::map<std::string, std::shared_ptr<Partition>> partitions_;
    std::map<std::string, std::string> indices_;  // map value has all attributes of a index
};


struct IndexParam {
    std::string table_name;
    std::string index_name;
    std::string index_type;
};

class Meta {
  public:
#define META_PERSIST_KEY_TABLES "META_PERSIST_KEY_TABLES"

#define INDEX_TYPE_ANNOY "annoy"
#define INDEX_TYPE_KNNGRAPH "knn_graph"

#define DISTANCE_TYPE_COSINE "cosine"
#define DISTANCE_TYPE_INNER_PRODUCT "inner_product"
#define DISTANCE_TYPE_EUCLIDEAN "euclidean"

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
    Status DropIndex(const std::string &name);
    Status ReplicaNameByKey(const std::string &table_name,
                            const std::string &key,
                            std::string &replica_name) const;

    jsonxx::json64 ToJson() const;

    std::string ToString() const {
        return ToJson().dump();
    }

    std::string ToStringPretty() const {
        return ToJson().dump(4, ' ');
    }

    Status ForEachTable(std::function<Status(std::shared_ptr<Table>)> func);
    Status ForEachPartition(std::function<Status(std::shared_ptr<Partition>)> func);
    Status ForEachReplica(std::function<Status(std::shared_ptr<Replica>)> func);
    Status ForEachReplica2(std::function<Status(std::shared_ptr<Table>, std::shared_ptr<Partition>, std::shared_ptr<Replica>)> func);
    Status ForEachReplicaOfTable(const std::string &table_name, std::function<Status(std::shared_ptr<Table>, std::shared_ptr<Partition>, std::shared_ptr<Replica>)> func);

    std::shared_ptr<Table>
    GetTable(const std::string &name) const;

    std::shared_ptr<Table>
    GetTableNonlocking(const std::string &name) const;

    std::shared_ptr<Partition>
    GetPartition(const std::string &name) const;

    std::shared_ptr<Partition>
    GetPartitionNonlocking(const std::string &name) const;

    std::shared_ptr<Replica>
    GetReplica(const std::string &name) const;

    std::map<std::string, std::shared_ptr<Table>>
    tables_copy() {
        std::unique_lock<std::mutex> guard(mutex_);
        return tables_;
    }

  private:
    std::map<std::string, std::shared_ptr<Table>> tables_;
    std::string path_;
    leveldb::DB* db_;

    mutable std::mutex mutex_;
};

} // namespace vectordb

#endif
