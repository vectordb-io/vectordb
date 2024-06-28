#include "metadata.h"

#include <vector>

#include "allocator.h"
#include "coding.h"
#include "common.h"
#include "leveldb/write_batch.h"
#include "util.h"
#include "vraft_logger.h"

namespace vectordb {

uint64_t Replica::replica_uid = 0;
uint64_t Partition::partition_uid = 0;
uint64_t Table::table_uid = 0;

std::string PartitionName(const std::string &table_name, int32_t partition_id) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s#%d", table_name.c_str(), partition_id);
  return std::string(buf);
}

std::string ReplicaName(const std::string &partition_name, int32_t replica_id) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s#%d", partition_name.c_str(), replica_id);
  return std::string(buf);
}

std::string ReplicaName(const std::string &table_name, int32_t partition_id,
                        int32_t replica_id) {
  return ReplicaName(PartitionName(table_name, partition_id), replica_id);
}

void ParsePartitionName(const std::string &partition_name,
                        std::string &table_name, int32_t &partition_id) {
  std::vector<std::string> result;
  vraft::Split(partition_name, '#', result);
  assert(result.size() == 2);
  table_name = result[0];
  partition_id = atoi(result[1].c_str());
}

void ParseReplicaName(const std::string &replica_name, std::string &table_name,
                      int32_t &partition_id, int32_t &replica_id) {
  std::vector<std::string> result;
  vraft::Split(replica_name, '#', result);
  assert(result.size() == 3);
  table_name = result[0];
  partition_id = atoi(result[1].c_str());
  replica_id = atoi(result[2].c_str());
}

int32_t Names::MaxBytes() {
  int32_t sz = 0;
  // names.size()
  sz += sizeof(uint32_t);

  // uids
  for (auto name : names) {
    sz += 2 * sizeof(uint32_t);
    sz += name.size();
  }
  return sz;
}

int32_t Names::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t Names::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  uint32_t u32 = names.size();
  vraft::EncodeFixed32(p, u32);
  p += sizeof(u32);
  size += sizeof(u32);

  // names
  for (auto item : names) {
    vraft::Slice sls(item.c_str(), item.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  assert(size <= len);
  return size;
}

int32_t Names::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t Names::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  int32_t table_num = vraft::DecodeFixed32(p);
  p += sizeof(table_num);
  size += sizeof(table_num);

  // names
  for (int32_t i = 0; i < table_num; ++i) {
    std::string table_name;
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      table_name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
    names.push_back(table_name);
  }

  return size;
}

nlohmann::json Names::ToJson() {
  nlohmann::json j;
  j[0]["size"] = names.size();

  int32_t i = 0;
  for (auto name : names) {
    j[1][i++] = name;
  }

  return j;
}

nlohmann::json Names::ToJsonTiny() { return ToJson(); }

std::string Names::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j = ToJsonTiny();
  } else {
    j = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

int32_t Replica::MaxBytes() {
  int32_t sz = 0;
  sz += sizeof(id);
  sz += 2 * sizeof(uint32_t);
  sz += name.size();
  sz += 2 * sizeof(uint32_t);
  sz += path.size();
  sz += sizeof(dim);
  sz += sizeof(uid);
  sz += 2 * sizeof(uint32_t);
  sz += table_name.size();
  sz += sizeof(table_uid);
  sz += 2 * sizeof(uint32_t);
  sz += partition_name.size();
  sz += sizeof(partition_uid);
  return sz;
}

int32_t Replica::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t Replica::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  vraft::EncodeFixed32(p, id);
  p += sizeof(id);
  size += sizeof(id);

  {
    vraft::Slice sls(name.c_str(), name.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  {
    vraft::Slice sls(path.c_str(), path.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  vraft::EncodeFixed32(p, dim);
  p += sizeof(dim);
  size += sizeof(dim);

  vraft::EncodeFixed64(p, uid);
  p += sizeof(uid);
  size += sizeof(uid);

  {
    vraft::Slice sls(table_name.c_str(), table_name.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  vraft::EncodeFixed64(p, table_uid);
  p += sizeof(table_uid);
  size += sizeof(table_uid);

  {
    vraft::Slice sls(partition_name.c_str(), partition_name.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  vraft::EncodeFixed64(p, partition_uid);
  p += sizeof(partition_uid);
  size += sizeof(partition_uid);

  assert(size <= len);
  return size;
}

int32_t Replica::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t Replica::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  id = vraft::DecodeFixed32(p);
  p += sizeof(id);
  size += sizeof(id);

  {
    name.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  {
    path.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      path.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  dim = vraft::DecodeFixed32(p);
  p += sizeof(dim);
  size += sizeof(dim);

  uid = vraft::DecodeFixed64(p);
  p += sizeof(uid);
  size += sizeof(uid);

  {
    table_name.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      table_name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  table_uid = vraft::DecodeFixed64(p);
  p += sizeof(table_uid);
  size += sizeof(table_uid);

  {
    partition_name.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      partition_name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  partition_uid = vraft::DecodeFixed64(p);
  p += sizeof(partition_uid);
  size += sizeof(partition_uid);

  return size;
}

nlohmann::json Replica::ToJson() {
  nlohmann::json j;
  j["id"] = id;
  j["name"] = name;
  j["path"] = path;
  j["dim"] = dim;
  j["uid"] = uid;
  j["table_name"] = table_name;
  j["table_uid"] = table_uid;
  j["partition_name"] = partition_name;
  j["partition_uid"] = partition_uid;
  return j;
}

nlohmann::json Replica::ToJsonTiny() { return ToJson(); }

std::string Replica::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["replica"] = ToJsonTiny();
  } else {
    j["replica"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

int32_t Partition::MaxBytes() {
  int32_t sz = 0;
  sz += sizeof(id);
  sz += 2 * sizeof(uint32_t);
  sz += name.size();
  sz += 2 * sizeof(uint32_t);
  sz += path.size();
  sz += sizeof(replica_num);
  sz += sizeof(dim);
  sz += sizeof(uid);

  sz += 2 * sizeof(uint32_t);
  sz += table_name.size();
  sz += sizeof(table_uid);

  // replicas_by_id.size()
  sz += sizeof(uint32_t);

  // ids
  for (auto item : replicas_by_id) {
    sz += sizeof(item.first);
  }

  // replicas_by_name.size()
  sz += sizeof(uint32_t);

  // names
  for (auto item : replicas_by_name) {
    sz += 2 * sizeof(uint32_t);
    sz += item.first.size();
  }

  return sz;
}

int32_t Partition::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t Partition::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  vraft::EncodeFixed32(p, id);
  p += sizeof(id);
  size += sizeof(id);

  {
    vraft::Slice sls(name.c_str(), name.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  {
    vraft::Slice sls(path.c_str(), path.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  vraft::EncodeFixed32(p, replica_num);
  p += sizeof(replica_num);
  size += sizeof(replica_num);

  vraft::EncodeFixed32(p, dim);
  p += sizeof(dim);
  size += sizeof(dim);

  vraft::EncodeFixed64(p, uid);
  p += sizeof(uid);
  size += sizeof(uid);

  {
    vraft::Slice sls(table_name.c_str(), table_name.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  vraft::EncodeFixed64(p, table_uid);
  p += sizeof(table_uid);
  size += sizeof(table_uid);

  // ids
  for (auto item : replicas_by_id) {
    int32_t i32 = item.first;
    vraft::EncodeFixed32(p, i32);
    p += sizeof(i32);
    size += sizeof(i32);
  }

  // names
  for (auto item : replicas_by_name) {
    vraft::Slice sls(item.first.c_str(), item.first.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  assert(size <= len);
  return size;
}

int32_t Partition::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t Partition::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  id = vraft::DecodeFixed32(p);
  p += sizeof(id);
  size += sizeof(id);

  {
    name.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  {
    path.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      path.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  replica_num = vraft::DecodeFixed32(p);
  p += sizeof(replica_num);
  size += sizeof(replica_num);

  dim = vraft::DecodeFixed32(p);
  p += sizeof(dim);
  size += sizeof(dim);

  uid = vraft::DecodeFixed64(p);
  p += sizeof(uid);
  size += sizeof(uid);

  {
    table_name.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      table_name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  table_uid = vraft::DecodeFixed64(p);
  p += sizeof(table_uid);
  size += sizeof(table_uid);

  // ids
  for (int32_t i = 0; i < replica_num; ++i) {
    int32_t i32 = vraft::DecodeFixed32(p);
    p += sizeof(i32);
    size += sizeof(i32);
    replicas_by_id[i32] = nullptr;
  }

  // names
  for (int32_t i = 0; i < replica_num; ++i) {
    std::string replica_name;
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      replica_name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
    replicas_by_name[replica_name] = nullptr;
  }

  return size;
}

nlohmann::json Partition::ToJson() {
  nlohmann::json j;
  j["id"] = id;
  j["name"] = name;
  j["path"] = path;
  j["replica_num"] = replica_num;
  j["dim"] = dim;
  j["uid"] = uid;
  j["table_name"] = table_name;
  j["table_uid"] = table_uid;

  int32_t i = 0;
  for (auto item : replicas_by_id) {
    j["replica_ids"][i++] = item.first;
  }

  i = 0;
  for (auto item : replicas_by_name) {
    j["replica_names"][i++] = item.first;
  }

  return j;
}

nlohmann::json Partition::ToJsonTiny() { return ToJson(); }

std::string Partition::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["partition"] = ToJsonTiny();
  } else {
    j["partition"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

int32_t Table::MaxBytes() {
  int32_t sz = 0;
  sz += 2 * sizeof(uint32_t);
  sz += name.size();
  sz += 2 * sizeof(uint32_t);
  sz += path.size();
  sz += sizeof(partition_num);
  sz += sizeof(replica_num);
  sz += sizeof(dim);
  sz += sizeof(uid);

  // partitions_by_id.size()
  sz += sizeof(uint32_t);

  // uids
  for (auto item : partitions_by_id) {
    sz += sizeof(item.first);
  }

  // replicas_by_name.size()
  sz += sizeof(uint32_t);

  // names
  for (auto item : partitions_by_name) {
    sz += 2 * sizeof(uint32_t);
    sz += item.first.size();
  }

  return sz;
}

int32_t Table::ToString(std::string &s) {
  s.clear();
  int32_t max_bytes = MaxBytes();
  char *ptr =
      reinterpret_cast<char *>(vraft::DefaultAllocator().Malloc(max_bytes));
  int32_t size = ToString(ptr, max_bytes);
  s.append(ptr, size);
  vraft::DefaultAllocator().Free(ptr);
  return size;
}

int32_t Table::ToString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    vraft::Slice sls(name.c_str(), name.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  {
    vraft::Slice sls(path.c_str(), path.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  vraft::EncodeFixed32(p, partition_num);
  p += sizeof(partition_num);
  size += sizeof(partition_num);

  vraft::EncodeFixed32(p, replica_num);
  p += sizeof(replica_num);
  size += sizeof(replica_num);

  vraft::EncodeFixed32(p, dim);
  p += sizeof(dim);
  size += sizeof(dim);

  vraft::EncodeFixed64(p, uid);
  p += sizeof(uid);
  size += sizeof(uid);

  // uids
  for (auto item : partitions_by_id) {
    int32_t i32 = item.first;
    vraft::EncodeFixed32(p, i32);
    p += sizeof(i32);
    size += sizeof(i32);
  }

  // names
  for (auto item : partitions_by_name) {
    vraft::Slice sls(item.first.c_str(), item.first.size());
    char *p2 = vraft::EncodeString2(p, len - size, sls);
    size += (p2 - p);
    p = p2;
  }

  assert(size <= len);
  return size;
}

int32_t Table::FromString(std::string &s) {
  return FromString(s.c_str(), s.size());
}

int32_t Table::FromString(const char *ptr, int32_t len) {
  char *p = const_cast<char *>(ptr);
  int32_t size = 0;

  {
    name.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  {
    path.clear();
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      path.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
  }

  partition_num = vraft::DecodeFixed32(p);
  p += sizeof(partition_num);
  size += sizeof(partition_num);

  replica_num = vraft::DecodeFixed32(p);
  p += sizeof(replica_num);
  size += sizeof(replica_num);

  dim = vraft::DecodeFixed32(p);
  p += sizeof(dim);
  size += sizeof(dim);

  uid = vraft::DecodeFixed64(p);
  p += sizeof(uid);
  size += sizeof(uid);

  // uids
  for (int32_t i = 0; i < partition_num; ++i) {
    int32_t i32 = vraft::DecodeFixed32(p);
    p += sizeof(i32);
    size += sizeof(i32);
    partitions_by_id[i32] = nullptr;
  }

  // names
  for (int32_t i = 0; i < partition_num; ++i) {
    std::string partition_name;
    vraft::Slice result;
    vraft::Slice input(p, len - size);
    int32_t sz = vraft::DecodeString2(&input, &result);
    if (sz > 0) {
      partition_name.append(result.data(), result.size());
      p += sz;
      size += sz;
    }
    partitions_by_name[partition_name] = nullptr;
  }

  return size;
}

nlohmann::json Table::ToJson() {
  nlohmann::json j;
  j["name"] = name;
  j["path"] = path;
  j["partition_num"] = partition_num;
  j["replica_num"] = replica_num;
  j["dim"] = dim;
  j["uid"] = uid;

#if 0
{
  int32_t i = 0;
  for (auto item : partitions_by_id) {
    j["partition_ids"][i++] = item.first;
  }
}
#endif

  {
    int32_t i = 0;
    for (auto item : partitions_by_name) {
      j["partition_names"][i++] = item.first;
    }
  }

  return j;
}

nlohmann::json Table::ToJsonTiny() { return ToJson(); }

std::string Table::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["table"] = ToJsonTiny();
  } else {
    j["table"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

Metadata::Metadata(const std::string &path) : path_(path) {
  int32_t rv = CreateDB();
  assert(rv == 0);

  rv = Load();
  assert(rv == 0);
}

nlohmann::json Metadata::ToJson() {
  nlohmann::json j;
  for (auto item : tables_) {
    j[item.first] = item.second->ToJson();
  }
  return j;
}

nlohmann::json Metadata::ToJsonTiny() { return ToJson(); }

std::string Metadata::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["meta"] = ToJsonTiny();
  } else {
    j["meta"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

int32_t Metadata::AddTable(TableParam param) {
  TableSPtr sptr = GetTable(param.name);
  if (sptr) {
    return -1;
  }

  sptr = CreateTable(param);
  tables_[sptr->name] = sptr;
  Persist();
  return 0;
}

TableSPtr Metadata::GetTable(const std::string &name) {
  TableSPtr sptr = nullptr;
  auto it = tables_.find(name);
  if (it != tables_.end()) {
    sptr = it->second;
  }
  return sptr;
}

PartitionSPtr Metadata::GetPartition(const std::string &name) {
  PartitionSPtr partition;
  std::string table_name;
  int32_t partition_id;
  ParsePartitionName(name, table_name, partition_id);
  TableSPtr table = GetTable(table_name);
  if (table) {
    auto it = table->partitions_by_id.find(partition_id);
    if (it != table->partitions_by_id.end()) {
      partition = it->second;
    }
  }
  return partition;
}

ReplicaSPtr Metadata::GetReplica(const std::string &name) {
  ReplicaSPtr replica;
  std::string table_name;
  int32_t partition_id;
  int32_t replica_id;
  ParseReplicaName(name, table_name, partition_id, replica_id);

  PartitionSPtr partition =
      GetPartition(PartitionName(table_name, partition_id));
  if (partition) {
    auto it = partition->replicas_by_id.find(replica_id);
    if (it != partition->replicas_by_id.end()) {
      replica = it->second;
      return replica;
    }
  }

  return replica;
}

void Metadata::ForEachTable(TableFunc func) {
  for (auto &table_kv : tables_) {
    func(table_kv.second);
  }
}

void Metadata::ForEachPartition(PartitionFunc func) {
  ForEachTable([func, this](TableSPtr sptr) {
    std::bind(&Metadata::ForEachPartitionInTable, this, sptr->name, func)();
  });
}

void Metadata::ForEachPartitionInTable(const std::string &table_name,
                                       PartitionFunc func) {
  TableSPtr table = GetTable(table_name);
  if (table) {
    for (auto &kv : table->partitions_by_name) {
      func(kv.second);
    }
  }
}

void Metadata::ForEachReplica(ReplicaFunc func) {
  ForEachTable([this, func](TableSPtr sptr) {
    std::bind(&Metadata::ForEachReplicaInTable, this, sptr->name, func)();
  });
}

void Metadata::ForEachReplicaInPartition(const std::string &partition_name,
                                         ReplicaFunc func) {
  PartitionSPtr partition = GetPartition(partition_name);
  if (partition) {
    for (auto &replica_kv : partition->replicas_by_id) {
      ReplicaSPtr replica = replica_kv.second;
      if (replica) {
        func(replica);
      }
    }
  }
}

void Metadata::ForEachReplicaInTable(const std::string &table_name,
                                     ReplicaFunc func) {
  ForEachPartitionInTable(table_name, [this, func](PartitionSPtr sptr) {
    std::bind(&Metadata::ForEachReplicaInPartition, this, sptr->name, func)();
  });
}

void Metadata::Tables(Names &names) {
  for (auto &kv : tables_) {
    names.names.push_back(kv.first);
  }
}

void Metadata::Partitions(Names &names) {
  ForEachPartition(
      [&names](PartitionSPtr p) { names.names.push_back(p->name); });
}

void Metadata::Replicas(Names &names) {
  ForEachReplica([&names](ReplicaSPtr r) { names.names.push_back(r->name); });
}

TableSPtr Metadata::CreateTable(TableParam param) {
  Table table;
  table.name = param.name;
  table.path = param.path + "/" + param.name;
  table.partition_num = param.partition_num;
  table.replica_num = param.replica_num;
  table.dim = param.dim;
  table.uid = Table::table_uid++;
  TableSPtr table_sptr = std::make_shared<Table>();
  *table_sptr = table;
  for (int32_t i = 0; i < table_sptr->partition_num; ++i) {
    Partition partition;
    partition.id = i;
    partition.name = PartitionName(table_sptr->name, i);
    partition.path = table_sptr->path + "/" + partition.name;
    partition.replica_num = table_sptr->replica_num;
    partition.dim = table_sptr->dim;
    partition.uid = Partition::partition_uid++;
    partition.table_name = table_sptr->name;
    partition.table_uid = table_sptr->uid;

    PartitionSPtr partition_sptr = CreatePartition(partition);
    (table_sptr->partitions_by_id)[partition_sptr->id] = partition_sptr;
    (table_sptr->partitions_by_name)[partition_sptr->name] = partition_sptr;
  }
  return table_sptr;
}

PartitionSPtr Metadata::CreatePartition(Partition param) {
  PartitionSPtr partition_sptr = std::make_shared<Partition>();
  *partition_sptr = param;
  for (int32_t i = 0; i < param.replica_num; ++i) {
    Replica replica;
    replica.id = i;
    replica.name = ReplicaName(param.name, i);
    replica.path = param.path + "/" + replica.name;
    replica.dim = param.dim;
    replica.uid = Replica::replica_uid++;
    replica.table_name = param.table_name;
    replica.table_uid = param.table_uid;
    replica.partition_name = param.name;
    replica.partition_uid = param.partition_uid;

    ReplicaSPtr replica_sptr = CreateReplica(replica);
    (partition_sptr->replicas_by_id)[replica_sptr->id] = replica_sptr;
    (partition_sptr->replicas_by_name)[replica_sptr->name] = replica_sptr;
  }
  return partition_sptr;
}

ReplicaSPtr Metadata::CreateReplica(Replica param) {
  ReplicaSPtr replica_sptr = std::make_shared<Replica>();
  *replica_sptr = param;
  return replica_sptr;
}

TableSPtr Metadata::LoadTable(const std::string &name) {
  TableSPtr sptr = nullptr;
  std::string value;
  auto s = db_->Get(leveldb::ReadOptions(), leveldb::Slice(name), &value);
  if (s.ok()) {
    Table t;
    t.FromString(value);
    sptr = std::make_shared<Table>();
    *sptr = t;
  }
  return sptr;
}

PartitionSPtr Metadata::LoadPartition(const std::string &name) {
  PartitionSPtr sptr;
  std::string value;
  auto s = db_->Get(leveldb::ReadOptions(), leveldb::Slice(name), &value);
  if (s.ok()) {
    Partition p;
    p.FromString(value);
    sptr = std::make_shared<Partition>();
    *sptr = p;
  }
  return sptr;
}

ReplicaSPtr Metadata::LoadReplica(const std::string &name) {
  ReplicaSPtr sptr;
  std::string value;
  auto s = db_->Get(leveldb::ReadOptions(), leveldb::Slice(name), &value);
  if (s.ok()) {
    Replica r;
    r.FromString(value);
    sptr = std::make_shared<Replica>();
    *sptr = r;
  }
  return sptr;
}

int32_t Metadata::CreateDB() {
  db_options_.create_if_missing = true;
  db_options_.error_if_exists = false;
  leveldb::DB *dbptr;
  leveldb::Status status = leveldb::DB::Open(db_options_, path_, &dbptr);
  if (!status.ok()) {
    vraft::vraft_logger.FError("leveldb open %s error, %s", path_.c_str(),
                               status.ToString().c_str());
    assert(0);
  }
  db_.reset(dbptr);
  return 0;
}

int32_t Metadata::Persist() {
  leveldb::WriteBatch batch;

  // table_names
  {
    Names tables;
    for (auto &item : tables_) {
      tables.names.push_back(item.first);
    }

    std::string key(METADATA_TABLES_KEY);
    std::string value;
    tables.ToString(value);

    batch.Put(leveldb::Slice(key), leveldb::Slice(value));
  }

  // tables
  {
    for (auto &t : tables_) {
      TableSPtr table_sptr = t.second;
      std::string table_key = table_sptr->name;
      std::string table_value;
      table_sptr->ToString(table_value);
      batch.Put(leveldb::Slice(table_key), leveldb::Slice(table_value));

      // partitions
      for (auto &p : table_sptr->partitions_by_name) {
        PartitionSPtr partition_sptr = p.second;
        std::string partition_key = partition_sptr->name;
        std::string partition_value;
        partition_sptr->ToString(partition_value);
        batch.Put(leveldb::Slice(partition_key),
                  leveldb::Slice(partition_value));

        // replicas
        for (auto &r : partition_sptr->replicas_by_name) {
          ReplicaSPtr replica_sptr = r.second;
          std::string replica_key = replica_sptr->name;
          std::string replica_value;
          replica_sptr->ToString(replica_value);
          batch.Put(leveldb::Slice(replica_key), leveldb::Slice(replica_value));
        }
      }
    }
  }

  // write
  leveldb::WriteOptions wo;
  wo.sync = true;
  leveldb::Status s = db_->Write(wo, &batch);
  assert(s.ok());

  return 0;
}

int32_t Metadata::Load() {
  std::string value;
  std::string key(METADATA_TABLES_KEY);
  auto s = db_->Get(leveldb::ReadOptions(), leveldb::Slice(key), &value);
  if (s.ok()) {
    Names t;
    t.FromString(value);
    for (auto table_name : t.names) {
      TableSPtr table_sptr = LoadTable(table_name);
      assert(table_sptr);
      tables_[table_sptr->name] = table_sptr;

      std::vector<std::string> tmp_partition_names;
      for (auto item : table_sptr->partitions_by_name) {
        tmp_partition_names.push_back(item.first);
      }

      for (auto &partition_name : tmp_partition_names) {
        PartitionSPtr partition_sptr = LoadPartition(partition_name);
        assert(partition_sptr);

        // add into table
        (table_sptr->partitions_by_name)[partition_sptr->name] = partition_sptr;
        (table_sptr->partitions_by_id)[partition_sptr->id] = partition_sptr;

        std::vector<std::string> tmp_replica_names;
        for (auto item : partition_sptr->replicas_by_name) {
          tmp_replica_names.push_back(item.first);
        }

        for (auto &replica_name : tmp_replica_names) {
          ReplicaSPtr replica_sptr = LoadReplica(replica_name);
          assert(replica_sptr);

          // add into partition
          (partition_sptr->replicas_by_name)[replica_sptr->name] = replica_sptr;
          (partition_sptr->replicas_by_id)[replica_sptr->id] = replica_sptr;
        }
      }
    }

  } else if (s.IsNotFound()) {
    // do nothing
    ;

  } else {
    assert(0);
  }

  return 0;
}

}  // namespace vectordb
