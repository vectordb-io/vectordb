#ifndef VRAFT_ECHO_CONSOLE_H_
#define VRAFT_ECHO_CONSOLE_H_

#include "console.h"

class EchoConsole : public vraft::Console {
 public:
  EchoConsole(const std::string &name, const std::string &addr)
      : Console(name, vraft::HostPort(addr)) {}
  ~EchoConsole() {}
  EchoConsole(const EchoConsole &t) = delete;
  EchoConsole &operator=(const EchoConsole &t) = delete;

 private:
  int32_t Parse(const std::string &cmd_line) override { return 0; }

  int32_t Execute() override {
    std::string msg = cmd_line();
    Send(msg);
    return 0;
  }

  void OnMessage(const vraft::TcpConnectionSPtr &conn,
                 vraft::Buffer *buf) override {
    set_result("msg back: " + std::string(buf->Peek()));
    buf->RetrieveAll();
    ResultReady();
  }
};

#endif
