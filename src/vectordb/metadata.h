#ifndef VECTORDB_METADATA_H_
#define VECTORDB_METADATA_H_

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "leveldb/db.h"
#include "nlohmann/json.hpp"
#include "vdb_common.h"

namespace vectordb {

std::string PartitionName(const std::string &table_name, int32_t partition_id);
std::string ReplicaName(const std::string &partition_name, int32_t replica_id);
std::string ReplicaName(const std::string &table_name, int32_t partition_id,
                        int32_t replica_id);

void ParsePartitionName(const std::string &partition_name,
                        std::string &table_name, int32_t &partition_id);
void ParseReplicaName(const std::string &replica_name, std::string &table_name,
                      int32_t &partition_id, int32_t &replica_id);

#define METADATA_TABLES_KEY "---|||---"

struct Names {
  std::vector<std::string> names;

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);
};

struct Replica {
  int32_t id;
  std::string name;
  std::string path;
  int32_t dim;
  uint64_t uid;

  std::string table_name;
  uint64_t table_uid;

  std::string partition_name;
  uint64_t partition_uid;

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  static uint64_t replica_uid;
};

struct Partition {
  int32_t id;
  std::string name;
  std::string path;
  int32_t replica_num;
  int32_t dim;
  uint64_t uid;

  std::string table_name;
  uint64_t table_uid;

  std::map<int32_t, ReplicaSPtr> replicas_by_id;
  std::map<std::string, ReplicaSPtr> replicas_by_name;

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  static uint64_t partition_uid;
};

// eg:
// path: /tmp/test
// name: test_table
struct TableParam {
  std::string name;
  std::string path;
  int32_t partition_num;
  int32_t replica_num;
  int32_t dim;
};

struct Table {
  std::string name;
  std::string path;
  int32_t partition_num;
  int32_t replica_num;
  int32_t dim;
  uint64_t uid;

  std::map<int32_t, PartitionSPtr> partitions_by_id;
  std::map<std::string, PartitionSPtr> partitions_by_name;

  int32_t MaxBytes();
  int32_t ToString(std::string &s);
  int32_t ToString(const char *ptr, int32_t len);
  int32_t FromString(std::string &s);
  int32_t FromString(const char *ptr, int32_t len);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  static uint64_t table_uid;
};

class Metadata final {
 public:
  explicit Metadata(const std::string &path);
  ~Metadata();
  Metadata(const Metadata &) = delete;
  Metadata &operator=(const Metadata &) = delete;

  int32_t AddTable(TableParam param);
  TableSPtr GetTable(const std::string &name);
  PartitionSPtr GetPartition(const std::string &name);
  ReplicaSPtr GetReplica(const std::string &name);

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  void ForEachTable(TableFunc func);
  void ForEachPartition(PartitionFunc func);
  void ForEachPartitionInTable(const std::string &table_name,
                               PartitionFunc func);
  void ForEachReplica(ReplicaFunc func);
  void ForEachReplicaInPartition(const std::string &partition_name,
                                 ReplicaFunc func);
  void ForEachReplicaInTable(const std::string &table_name, ReplicaFunc func);

  void Tables(Names &names);
  void Partitions(Names &names);
  void Replicas(Names &names);

 private:
  TableSPtr CreateTable(TableParam param);
  PartitionSPtr CreatePartition(Partition param);
  ReplicaSPtr CreateReplica(Replica param);
  TableSPtr LoadTable(const std::string &name);
  PartitionSPtr LoadPartition(const std::string &name);
  ReplicaSPtr LoadReplica(const std::string &name);
  int32_t CreateDB();
  int32_t Persist();
  int32_t Load();

 private:
  std::string path_;
  leveldb::Options db_options_;
  std::shared_ptr<leveldb::DB> db_;
  std::unordered_map<std::string, TableSPtr> tables_;
};

inline Metadata::~Metadata() {}

}  // namespace vectordb

#endif
