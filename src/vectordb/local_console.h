#ifndef VECTORDB_LOCAL_CONSOLE_H_
#define VECTORDB_LOCAL_CONSOLE_H_

#include <memory>

#include "console.h"
#include "cxxopts.hpp"
#include "vdb_common.h"

namespace vectordb {

class LocalConsole;
using LocalConsoleSPtr = std::shared_ptr<LocalConsole>;
using LocalConsoleUPtr = std::unique_ptr<LocalConsole>;
using LocalConsoleWPtr = std::weak_ptr<LocalConsole>;

class LocalConsole final : public vraft::Console {
 public:
  explicit LocalConsole(const std::string &name, const std::string &path);
  ~LocalConsole();
  LocalConsole(const LocalConsole &) = delete;
  LocalConsole &operator=(const LocalConsole &) = delete;

 private:
  int32_t Parse(const std::string &cmd_line) override;
  int32_t Execute() override;
  void OnMessage(const vraft::TcpConnectionSPtr &conn,
                 vraft::Buffer *buf) override {}

 private:
  void Clear();
  void Help();
  void Error();
  void Quit();
  void Version();
  void Meta();
  void CreateTable();
  void Load();
  void GetKNN();
  void BuildIndex();
  void ShowTables();
  void ShowPartitions();
  void ShowReplicas();
  void DescTable();
  void DescPartition();
  void DescDescReplica();
  void DescDescEngine();
  void Get();
  void Put();
  void Delete();

 private:
  // parse result
  ParserSPtr parser_;
  VdbEngineSPtr vdb_;
};

inline LocalConsole::~LocalConsole() {}

}  // namespace vectordb

#endif
