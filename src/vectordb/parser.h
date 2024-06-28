#ifndef VECTORDB_PARSER_H_
#define VECTORDB_PARSER_H_

#include <string>
#include <vector>

#include "cxxopts.hpp"
#include "nlohmann/json.hpp"

namespace vectordb {

enum VectordbCmd {
  kCmdError = 0,
  kCmdHelp,     // help
  kCmdVersion,  // version
  kCmdQuit,     // quit
  kCmdMeta,     // meta
  kCmdPut,      // put
  kCmdGet,      // get
  kCmdDelete,   // del
  kCmdGetKNN,   // getknn
  kCmdGetKNN2,  // getknn
  kCmdLoad,     // load

  kCmdCreateTable,   // create table
  kCmdBuildIndex,    // build index
  kDescTable,        // desc table
  kDescPartition,    // desc partition
  kDescDescReplica,  // desc replica
  kDescDescEngine,   // desc engine
  kShowTables,       // show tables
  kShowPartitions,   // show partitions
  kShowReplicas,     // show replicas

  kCmdEnd,
};

const std::string HelpStr();

const std::string example_cmdstr(VectordbCmd cmd);

std::string CmdStr(VectordbCmd cmd);

std::string MergeString(const std::vector<std::string> &strs);

// get cmd, generate argc and argv
VectordbCmd GetCmd(const std::string &cmd_line, int *argc, char ***argv);

class Parser {
 public:
  Parser(const std::string &cmd_line);
  ~Parser();
  Parser(const Parser &) = delete;
  Parser &operator=(const Parser &) = delete;

  nlohmann::json ToJson();
  nlohmann::json ToJsonTiny();
  std::string ToJsonString(bool tiny, bool one_line);

  VectordbCmd cmd() const { return cmd_; }
  std::string name() const { return name_; }
  int32_t partition_num() const { return partition_num_; }
  int32_t replica_num() const { return replica_num_; }
  int32_t dim() const { return dim_; }
  int32_t annoy_tree_num() const { return annoy_tree_num_; }
  std::string key() const { return key_; }
  std::vector<float> &vec() { return vec_; }
  std::string attach_value() const { return attach_value_; }
  std::string table() const { return table_; }
  int32_t limit() const { return limit_; }
  std::string file() const { return file_; }

 private:
  void Parse();

 private:
  int32_t argc_;
  char **argv_;
  VectordbCmd cmd_;
  std::string cmd_line_;
  std::shared_ptr<cxxopts::Options> options_;
  std::shared_ptr<cxxopts::ParseResult> parse_result_;

  std::string name_;
  int32_t partition_num_;
  int32_t replica_num_;
  int32_t dim_;
  int32_t annoy_tree_num_;
  std::string key_;
  std::vector<float> vec_;
  std::string attach_value_;
  std::string table_;
  int32_t limit_;
  std::string file_;
};

}  // namespace vectordb

#endif
