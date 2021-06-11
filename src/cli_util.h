#ifndef __VECTORDB_CLI_UTIL_H__
#define __VECTORDB_CLI_UTIL_H__

#include <vector>
#include <string>
#include "vectordb_rpc.grpc.pb.h"

namespace vectordb {

std::string HelpStr();
void ToLower(std::string &str);
void Parse(std::string &line, std::vector<std::string> &argv);

std::string ToString(const vectordb_rpc::PingReply &msg);

}  // namespace vectordb

#endif
