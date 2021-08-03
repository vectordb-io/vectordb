#ifndef __VECTORDB_UTIL_H__
#define __VECTORDB_UTIL_H__

#include <vector>
#include <string>

namespace vectordb {

namespace util {

unsigned int RSHash(const char *str);
void Split(const std::string &s, char separator, std::vector<std::string> &sv, const std::string ignore = "");

void ToLower(std::string &str);
bool DirOK(const std::string &path);
bool Mkdir(const std::string &path);

std::string PartitionName(const std::string &table_name, int partition_id);
std::string ReplicaName(const std::string &table_name, int partition_id, int replica_id);
bool ParsePartitionName(const std::string &partition_name, std::string &table_name, int &partition_id);
bool ParseReplicaName(const std::string &replica_name, std::string &table_name, int &partition_id, int &replica_id);

bool Distance(const std::vector<double> &v1, const std::vector<double> &v2, double &d);

} // namespace util

} // namespace vectordb

#endif
