#ifndef __VECTORDB_UTIL_H__
#define __VECTORDB_UTIL_H__

#include <ctime>
#include <vector>
#include <string>

namespace vectordb {

namespace util {

bool RemoveDir(const std::string &path);
bool ChildrenOfDir(const std::string &path, std::vector<std::string> &children_paths, std::vector<std::string> &children_names);
std::string LocalTimeString(time_t t);
unsigned int RSHash(const char *str);
void Split(const std::string &s, char separator, std::vector<std::string> &sv, const std::string ignore = "");

void ToLower(std::string &str);
bool DirOK(const std::string &path);
bool MakeDir(const std::string &path);
bool RecurMakeDir(const std::string &path);

std::string PartitionName(const std::string &table_name, int partition_id);
std::string ReplicaName(const std::string &table_name, int partition_id, int replica_id);
std::string IndexName(const std::string &table_name, const std::string &index_type, time_t timestamp);
bool ParsePartitionName(const std::string &partition_name, std::string &table_name, int &partition_id);
bool ParseReplicaName(const std::string &replica_name, std::string &table_name, int &partition_id, int &replica_id);
bool ParseIndexName(const std::string &index_name, std::string &table_name, std::string &index_type, time_t &timestamp);

bool Distance(const std::vector<float> &v1, const std::vector<float> &v2, float &d);

} // namespace util

} // namespace vectordb

#endif
