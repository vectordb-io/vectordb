#ifndef __VECTORDB_CLI_UTIL_H__
#define __VECTORDB_CLI_UTIL_H__

#include <set>
#include <vector>
#include <string>
#include "vectordb_rpc.grpc.pb.h"

namespace vectordb {

namespace cli_util {

std::string HelpStr();
void ToLower(std::string &str);
void Split(const std::string &s, char separator, std::vector<std::string> &sv, const std::string ignore = "");
void Split2(const std::string &s, char separator, std::string &s1, std::string &s2);

std::string ToString(const vectordb_rpc::PingReply &reply);
std::string ToString(const vectordb_rpc::CreateTableReply &reply);
std::string ToString(const vectordb_rpc::ShowTablesReply &reply);

} // namespace cli_util

}  // namespace vectordb

#endif
