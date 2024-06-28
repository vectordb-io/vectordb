#ifndef VRAFT_CONSOLE_H_
#define VRAFT_CONSOLE_H_

#include <atomic>
#include <string>

#include "buffer.h"
#include "common.h"
#include "count_down.h"
#include "hostport.h"

namespace vraft {

class Console {
 public:
  explicit Console(const std::string &name);
  explicit Console(const std::string &name, const HostPort &dest);
  virtual ~Console();
  Console(const Console &t) = delete;
  Console &operator=(const Console &t) = delete;

  int32_t Run();
  void Stop();
  void Reset();

  void set_result(const std::string &result);
  std::string cmd_line() const;

 protected:
  void WaitResult();
  void ResultReady();
  void Send(std::string &msg);

 private:
  virtual int32_t Parse(const std::string &cmd_line) = 0;
  virtual int32_t Execute() = 0;
  virtual void OnMessage(const TcpConnectionSPtr &conn, Buffer *buf) = 0;

 private:
  std::atomic<bool> start;

  std::string name_;
  std::string prompt_;
  std::string cmd_line_;

  std::string result_;
  CountDownLatch wait_result_;
  HostPort dest_;
  ClientThreadSPtr client_thread_;
};

inline Console::~Console() {}

}  // namespace vraft

#endif
