#include "parser.h"

#include "common.h"
#include "util.h"

namespace vectordb {

const std::string HelpStr() {
  std::string help;
  help.append("example:\n\n");
  for (int32_t i = kCmdHelp; i < kCmdEnd; ++i) {
    help.append(example_cmdstr(*(VectordbCmd *)(&i))).append("\n");
  }
  return help;
}

const std::string example_cmdstr(VectordbCmd cmd) {
  // char str_buf[1024];
  std::string cmd_str;
  switch (cmd) {
    case kCmdHelp: {
      cmd_str.append("help");
      return cmd_str;
    }

    case kCmdVersion: {
      cmd_str.append("version");
      return cmd_str;
    }

    case kCmdQuit: {
      cmd_str.append("quit");
      return cmd_str;
    }

    case kCmdMeta: {
      cmd_str.append("meta");
      return cmd_str;
    }

    case kCmdPut: {
      cmd_str.append(
          "put --table=test-table --key=key_0 "
          "--vec=0.435852,0.031869,0.161108,0.055670,0.846847,0.604385,0."
          "075282,0.386435,0.407000,0.101307 "
          "--attach_value=aaavvv");
      return cmd_str;
    }

    case kCmdGet: {
      cmd_str.append("get --table=test-table --key=key_0");
      return cmd_str;
    }

    case kCmdDelete: {
      cmd_str.append("del --table=test-table --key=key_0");
      return cmd_str;
    }

    case kCmdGetKNN: {
      cmd_str.append("getknn --table=test-table --key=key_0 --limit=20");
      return cmd_str;
    }

    case kCmdGetKNN2: {
      cmd_str.append(
          "getknn --table=test-table "
          "--vec=0.435852,0.031869,0.161108,0.055670,0.846847,0.604385,0."
          "075282,0.386435,0.407000,0.101307 "
          "--limit=20");
      return cmd_str;
    }

    case kCmdLoad: {
      cmd_str.append("load --table=test-table --file=/tmp/vec.txt");
      return cmd_str;
    }

    case kCmdCreateTable: {
      cmd_str.append(
          "create table --name=test-table --partition_num=10 --replica_num=3 "
          "--dim=10");
      return cmd_str;
    }

    case kCmdBuildIndex: {
      cmd_str.append("build index --table=test-table --annoy_tree_num=10");
      return cmd_str;
    }

    case kDescTable: {
      cmd_str.append("desc table test-table");
      return cmd_str;
    }

    case kDescPartition: {
      cmd_str.append("desc partition test-table#0");
      return cmd_str;
    }

    case kDescDescReplica: {
      cmd_str.append("desc replica test-table#0#0");
      return cmd_str;
    }

    case kDescDescEngine: {
      cmd_str.append("desc engine test-table#0#0");
      return cmd_str;
    }

    case kShowTables: {
      cmd_str.append("show tables");
      return cmd_str;
    }

    case kShowPartitions: {
      cmd_str.append("show partitions");
      return cmd_str;
    }

    case kShowReplicas: {
      cmd_str.append("show replicas");
      return cmd_str;
    }

    default:
      return cmd_str;
  }
}

std::string CmdStr(VectordbCmd cmd) {
  switch (cmd) {
    case kCmdError:
      return "kCmdError";
    case kCmdHelp:
      return "kCmdHelp";
    case kCmdVersion:
      return "kCmdVersion";
    case kCmdQuit:
      return "kCmdQuit";
    case kCmdMeta:
      return "kMeta";
    case kCmdPut:
      return "kCmdPut";
    case kCmdGet:
      return "kCmdGet";
    case kCmdDelete:
      return "kCmdDelete";
    case kCmdGetKNN:
      return "kCmdGetKNN";
    case kCmdGetKNN2:
      return "kCmdGetKNN2";
    case kCmdLoad:
      return "kCmdLoad";
    case kCmdCreateTable:
      return "kCmdCreateTable";
    case kCmdBuildIndex:
      return "kCmdBuildIndex";
    case kDescTable:
      return "kDescTable";
    case kDescPartition:
      return "kDescPartition";
    case kDescDescReplica:
      return "kDescDescReplica";
    case kDescDescEngine:
      return "kDescDescEngine";
    case kShowTables:
      return "kShowTables";
    case kShowPartitions:
      return "kShowPartitions";
    case kShowReplicas:
      return "kShowReplicas";
    default:
      return "kCmdError";
  }
}

std::string MergeString(const std::vector<std::string> &strs) {
  std::string str;
  for (auto &s : strs) {
    str.append(s).append(" ");
  }
  return str;
}

VectordbCmd GetCmd(const std::string &cmd_line, int *argc, char ***argv) {
  VectordbCmd ret_cmd = kCmdError;
  size_t del_count = 0;
  std::string cmd_line2;
  std::vector<std::string> result;
  vraft::Split(cmd_line, ' ', result);
  if (result.size() == 0) {
    return kCmdError;
  }

  if (result[0] == "create") {
    del_count = 2;
    ret_cmd = kCmdCreateTable;
    std::string cmd_arg2 = "table";

    if (result[1] == cmd_arg2 && result.size() > del_count) {
      result.erase(result.begin(), result.begin() + del_count);
      cmd_line2.append(CmdStr(ret_cmd)).append(" ").append(MergeString(result));
      vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "build") {
    del_count = 2;
    ret_cmd = kCmdBuildIndex;
    std::string cmd_arg2 = "index";

    if (result[1] == cmd_arg2 && result.size() > del_count) {
      result.erase(result.begin(), result.begin() + del_count);
      cmd_line2.append(CmdStr(ret_cmd)).append(" ").append(MergeString(result));
      vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "desc") {
    if (result.size() == 3) {
      if (result[1] == "table") {
        ret_cmd = kDescTable;

        cmd_line2.append(CmdStr(ret_cmd)).append(" --name=").append(result[2]);
        vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
        return ret_cmd;

      } else if (result[1] == "partition") {
        ret_cmd = kDescPartition;

        cmd_line2.append(CmdStr(ret_cmd)).append(" --name=").append(result[2]);
        vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
        return ret_cmd;

      } else if (result[1] == "replica") {
        ret_cmd = kDescDescReplica;

        cmd_line2.append(CmdStr(ret_cmd)).append(" --name=").append(result[2]);
        vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
        return ret_cmd;

      } else if (result[1] == "engine") {
        ret_cmd = kDescDescEngine;

        cmd_line2.append(CmdStr(ret_cmd)).append(" --name=").append(result[2]);
        vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
        return ret_cmd;

      } else {
        return kCmdError;
      }
    }

  } else if (result[0] == "show") {
    if (result.size() == 2) {
      if (result[1] == "tables") {
        ret_cmd = kShowTables;
        cmd_line2.append(CmdStr(ret_cmd));
        vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
        return ret_cmd;
      } else if (result[1] == "partitions") {
        ret_cmd = kShowPartitions;
        cmd_line2.append(CmdStr(ret_cmd));
        vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
        return ret_cmd;
      } else if (result[1] == "replicas") {
        ret_cmd = kShowReplicas;
        cmd_line2.append(CmdStr(ret_cmd));
        vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
        return ret_cmd;
      }
    }

  } else if (result[0] == "help") {
    if (result.size() == 1) {
      ret_cmd = kCmdHelp;
      cmd_line2.append(CmdStr(ret_cmd));
      vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "version") {
    if (result.size() == 1) {
      ret_cmd = kCmdVersion;
      cmd_line2.append(CmdStr(ret_cmd));
      vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "quit") {
    if (result.size() == 1) {
      ret_cmd = kCmdQuit;
      cmd_line2.append(CmdStr(ret_cmd));
      vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "meta") {
    if (result.size() == 1) {
      ret_cmd = kCmdMeta;
      cmd_line2.append(CmdStr(ret_cmd));
      vraft::ConvertStringToArgcArgv(cmd_line2, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "put") {
    if (result.size() == 4 || result.size() == 5) {
      ret_cmd = kCmdPut;
      vraft::ConvertStringToArgcArgv(cmd_line, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "get") {
    if (result.size() == 3) {
      ret_cmd = kCmdGet;
      vraft::ConvertStringToArgcArgv(cmd_line, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "del") {
    if (result.size() == 3) {
      ret_cmd = kCmdDelete;
      vraft::ConvertStringToArgcArgv(cmd_line, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "getknn") {
    if (result.size() == 4) {
      ret_cmd = kCmdGetKNN;
      vraft::ConvertStringToArgcArgv(cmd_line, argc, argv);
      return ret_cmd;
    }

  } else if (result[0] == "load") {
    if (result.size() == 3) {
      ret_cmd = kCmdLoad;
      vraft::ConvertStringToArgcArgv(cmd_line, argc, argv);
      return ret_cmd;
    }
  }

  return kCmdError;
}

nlohmann::json Parser::ToJson() {
  nlohmann::json j;
  j["cmd_line"] = cmd_line_;
  j["cmd"] = CmdStr(cmd_);
  for (int32_t i = 0; i < argc_; ++i) {
    j["argvs"][i] = argv_[i];
  }

  j["param"]["name"] = name();
  j["param"]["partition_num"] = partition_num();
  j["param"]["replica_num"] = replica_num();
  j["param"]["dim"] = dim();
  j["param"]["annoy_tree_num"] = annoy_tree_num();
  j["param"]["key"] = key();
  j["param"]["attach_value"] = attach_value();
  j["param"]["vec"] = vec_;
  j["param"]["table"] = table_;
  j["param"]["limit"] = limit_;

  return j;
}

nlohmann::json Parser::ToJsonTiny() { return ToJson(); }

std::string Parser::ToJsonString(bool tiny, bool one_line) {
  nlohmann::json j;
  if (tiny) {
    j["parser"] = ToJsonTiny();
  } else {
    j["parser"] = ToJson();
  }

  if (one_line) {
    return j.dump();
  } else {
    return j.dump(JSON_TAB);
  }
}

void Parser::Parse() {
  vraft::DelHead(cmd_line_, " ");
  vraft::DelTail(cmd_line_, " ;");
  cmd_ = GetCmd(cmd_line_, &argc_, &argv_);

  if (cmd_ != kCmdError) {
    options_ = std::make_shared<cxxopts::Options>(std::string(argv_[0]));
    options_->add_options()("name", "name",
                            cxxopts::value<std::string>()->default_value(""))(
        "partition_num", "partition_num",
        cxxopts::value<int32_t>()->default_value("0"))(
        "replica_num", "replica_num",
        cxxopts::value<int32_t>()->default_value("0"))(
        "dim", "dim", cxxopts::value<int32_t>()->default_value("0"))(
        "annoy_tree_num", "annoy_tree_num",
        cxxopts::value<int32_t>()->default_value("10"))(
        "key", "key", cxxopts::value<std::string>()->default_value(""))(
        "attach_value", "attach_value",
        cxxopts::value<std::string>()->default_value(""))(
        "vec", "vec", cxxopts::value<std::string>()->default_value(""))(
        "table", "table", cxxopts::value<std::string>()->default_value(""))(
        "limit", "limit", cxxopts::value<int32_t>()->default_value("0"))(
        "file", "file", cxxopts::value<std::string>()->default_value(""));

    parse_result_ = std::make_shared<cxxopts::ParseResult>();
    *parse_result_ = options_->parse(argc_, argv_);

    if (parse_result_->count("name")) {
      name_ = (*parse_result_)["name"].as<std::string>();
    }

    if (parse_result_->count("partition_num")) {
      partition_num_ = (*parse_result_)["partition_num"].as<int32_t>();
    }

    if (parse_result_->count("replica_num")) {
      replica_num_ = (*parse_result_)["replica_num"].as<int32_t>();
    }

    if (parse_result_->count("dim")) {
      dim_ = (*parse_result_)["dim"].as<int32_t>();
    }

    if (parse_result_->count("annoy_tree_num")) {
      annoy_tree_num_ = (*parse_result_)["annoy_tree_num"].as<int32_t>();
    }

    if (parse_result_->count("key")) {
      key_ = (*parse_result_)["key"].as<std::string>();
    }

    if (parse_result_->count("attach_value")) {
      attach_value_ = (*parse_result_)["attach_value"].as<std::string>();
    }

    if (parse_result_->count("table")) {
      table_ = (*parse_result_)["table"].as<std::string>();
    }

    if (parse_result_->count("vec")) {
      std::string float_vec_str = (*parse_result_)["vec"].as<std::string>();

      std::vector<std::string> result;
      vraft::Split(float_vec_str, ',', result);
      for (auto &item : result) {
        float f32;
        sscanf(item.c_str(), "%f", &f32);
        vec_.push_back(f32);
      }
    }

    if (parse_result_->count("limit")) {
      limit_ = (*parse_result_)["limit"].as<int32_t>();
    }

    if (parse_result_->count("file")) {
      file_ = (*parse_result_)["file"].as<std::string>();
    }
  }
}

Parser::Parser(const std::string &cmd_line)
    : argc_(0),
      argv_(nullptr),
      cmd_line_(cmd_line),
      name_(""),
      partition_num_(0),
      replica_num_(0),
      dim_(0),
      annoy_tree_num_(0),
      limit_(0) {
  Parse();
}

Parser::~Parser() { vraft::FreeArgv(argc_, argv_); }

}  // namespace vectordb
