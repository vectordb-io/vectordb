#ifndef VRAFT_TCP_SERVER_H_
#define VRAFT_TCP_SERVER_H_

#include <map>
#include <memory>
#include <string>

#include "acceptor.h"
#include "eventloop.h"
#include "hostport.h"
#include "tcp_connection.h"
#include "tcp_options.h"
#include "timer.h"

namespace vraft {

class TcpServer final {
 public:
  TcpServer(EventLoopSPtr &loop, const std::string &name,
            const HostPort &listen_addr, const TcpOptions &options);
  ~TcpServer();
  TcpServer(const TcpServer &t) = delete;
  TcpServer &operator=(const TcpServer &t) = delete;

  // call in any thread
  void Stop();
  void RunFunctor(const Functor func);

  // call in loop thread
  void AssertInLoopThread();
  std::string DebugString() const;

  // control
  int32_t Start();
  bool Active();
  void AddConnection(TcpConnectionSPtr &conn);
  void RemoveConnection(const TcpConnectionSPtr &conn);

  // set/get
  void set_on_connection_cb(const OnConnectionCallback &cb);
  void set_write_complete_cb(const WriteCompleteCallback &cb);
  void set_on_message_cb(const OnMessageCallback &cb);
  void set_close_cb(const Functor &cb);
  const std::string &name() const;

 private:
  void Init();
  void NewConnection(UvTcpUPtr client);
  int32_t Close();

 private:
  const std::string name_;
  EventLoopWPtr loop_;

  ConnectionMap connections_;
  Acceptor acceptor_;

  OnMessageCallback on_message_cb_;
  OnConnectionCallback on_connection_cb_;
  WriteCompleteCallback write_complete_cb_;
  Functor close_cb_;
};

// call in loop thread
inline bool TcpServer::Active() { return acceptor_.Active(); }

inline void TcpServer::set_on_connection_cb(const OnConnectionCallback &cb) {
  on_connection_cb_ = cb;
}

inline void TcpServer::set_write_complete_cb(const WriteCompleteCallback &cb) {
  write_complete_cb_ = cb;
}

inline void TcpServer::set_on_message_cb(const OnMessageCallback &cb) {
  on_message_cb_ = cb;
}

inline void TcpServer::set_close_cb(const Functor &cb) { close_cb_ = cb; }

inline const std::string &TcpServer::name() const { return name_; }

}  // namespace vraft

#endif
