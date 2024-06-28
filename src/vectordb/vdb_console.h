#ifndef VECTORDB_VDB_CONSOLE_H_
#define VECTORDB_VDB_CONSOLE_H_

#include <memory>

#include "console.h"
#include "cxxopts.hpp"
#include "msg_version_reply.h"

namespace vectordb {

class VdbConsole;
using VdbConsoleSPtr = std::shared_ptr<VdbConsole>;
using VdbConsoleUPtr = std::unique_ptr<VdbConsole>;
using VdbConsoleWPtr = std::weak_ptr<VdbConsole>;

class VdbConsole : public vraft::Console {
 public:
  VdbConsole(const std::string &name, const std::string &addr)
      : Console(name, vraft::HostPort(addr)),
        argc_(0),
        argv_(nullptr),
        cmd_(""),
        options_(nullptr) {}
  ~VdbConsole() {}
  VdbConsole(const VdbConsole &t) = delete;
  VdbConsole &operator=(const VdbConsole &t) = delete;

 private:
  int32_t Parse(const std::string &cmd_line) override;
  int32_t Execute() override;
  void OnMessage(const vraft::TcpConnectionSPtr &conn,
                 vraft::Buffer *buf) override;

 private:
  void OnVersionReply(const MsgVersionReply &msg);
  void Clear();
  void Help();
  void Error();
  void Quit();
  void Version();

 private:
  // parse result
  int argc_;
  char **argv_;
  std::string cmd_;
  std::shared_ptr<cxxopts::Options> options_;
  cxxopts::ParseResult parse_result_;
};

}  // namespace vectordb

#endif
