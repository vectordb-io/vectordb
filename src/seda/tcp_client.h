#ifndef VRAFT_TCP_CLIENT_H_
#define VRAFT_TCP_CLIENT_H_

#include <memory>
#include <string>

#include "common.h"
#include "connector.h"
#include "eventloop.h"
#include "tcp_connection.h"
#include "tcp_options.h"

namespace vraft {

class TcpClient final {
 public:
  TcpClient(EventLoopSPtr &loop, const std::string name,
            const HostPort &dest_addr, const TcpOptions &options);
  ~TcpClient();
  TcpClient(const TcpClient &t) = delete;
  TcpClient &operator=(const TcpClient &t) = delete;

  // call in any thread
  void Stop();
  void RunFunctor(const Functor func);

  // call in loop thread
  void AssertInLoopThread();
  std::string DebugString() const;
  std::string ToString() const;

  // connect
  int32_t TimerConnect(int64_t retry_times);
  int32_t Connect(int64_t retry_times);
  int32_t Connect();

  // connection
  bool Connected() const;
  void RemoveConnection(const TcpConnectionSPtr &conn);

  // send
  int32_t Send(const char *buf, unsigned int size);
  int32_t CopySend(const char *buf, unsigned int size);
  int32_t BufSend(const char *buf, unsigned int size);

  // set/get
  const std::string &name() const;
  const HostPort dest_addr() const;
  void set_on_message_cb(const OnMessageCallback &cb);
  void set_on_connection_cb(const OnConnectionCallback &cb);
  void set_write_complete_cb(const WriteCompleteCallback &cb);
  void set_connection_close_cb(const ConnectionCloseCallback &cb);
  void set_close_cb(const Functor &cb);

 private:
  void Init();
  void NewConnection(UvTcpUPtr client);
  int32_t Close();

 private:
  const std::string name_;
  HostPort dest_addr_;
  EventLoopWPtr loop_;

  Connector connector_;
  TcpConnectionSPtr connection_;

  OnMessageCallback on_message_cb_;
  OnConnectionCallback on_connection_cb_;
  WriteCompleteCallback write_complete_cb_;
  ConnectionCloseCallback connection_close_cb_;
  Functor close_cb_;
};

inline void TcpClient::set_on_message_cb(const OnMessageCallback &cb) {
  on_message_cb_ = cb;
}

inline void TcpClient::set_on_connection_cb(const OnConnectionCallback &cb) {
  on_connection_cb_ = cb;
}

inline void TcpClient::set_write_complete_cb(const WriteCompleteCallback &cb) {
  write_complete_cb_ = cb;
}

inline void TcpClient::set_connection_close_cb(
    const ConnectionCloseCallback &cb) {
  connection_close_cb_ = cb;
}

inline void TcpClient::set_close_cb(const Functor &cb) { close_cb_ = cb; }

inline const std::string &TcpClient::name() const { return name_; }

inline const HostPort TcpClient::dest_addr() const { return dest_addr_; }

}  // namespace vraft

#endif