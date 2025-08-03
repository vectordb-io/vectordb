#ifndef VRAFT_ACCEPTOR_H_
#define VRAFT_ACCEPTOR_H_

#include <atomic>

#include "common.h"
#include "hostport.h"
#include "tcp_connection.h"
#include "tcp_options.h"

namespace vraft {

using AcceptorNewConnFunc = std::function<void(UvTcpUPtr)>;
void AcceptorHandleRead(UvStream *server, int status);
void AcceptorCloseCb(UvHandle *handle);

class Acceptor final {
 public:
  Acceptor(EventLoopSPtr &loop, const HostPort &addr,
           const TcpOptions &options);
  ~Acceptor();
  Acceptor(const Acceptor &a) = delete;
  Acceptor &operator=(const Acceptor &a) = delete;

  // call in loop thread
  void AssertInLoopThread() const;
  std::string DebugString() const;

  // control
  int32_t Start();
  int32_t Close();
  bool Active();
  void NewConnection(UvTcpUPtr conn);

  // set/get
  UvLoop *UvLoopPtr();
  const HostPort &addr() const;
  const TcpOptions &options() const;
  void set_new_conn_func(const AcceptorNewConnFunc &new_conn_func);
  void set_close_cb(const Functor &close_cb);

 private:
  void Init();
  int32_t Bind();
  int32_t Listen();

 private:
  const HostPort addr_;
  const TcpOptions options_;
  AcceptorNewConnFunc new_conn_func_;
  Functor close_cb_;

  UvTcp server_;
  EventLoopWPtr loop_;

  friend void AcceptorHandleRead(UvStream *server, int status);
  friend void AcceptorCloseCb(UvHandle *handle);
};

inline const HostPort &Acceptor::addr() const { return addr_; }

inline const TcpOptions &Acceptor::options() const { return options_; }

inline void Acceptor::set_new_conn_func(
    const AcceptorNewConnFunc &new_conn_func) {
  new_conn_func_ = new_conn_func;
}

inline void Acceptor::set_close_cb(const Functor &close_cb) {
  close_cb_ = close_cb;
}

}  // namespace vraft

#endif
