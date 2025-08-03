#ifndef VRAFT_CLIENT_THREAD_H_
#define VRAFT_CLIENT_THREAD_H_

#include <vector>

#include "count_down.h"
#include "loop_thread.h"
#include "tcp_client.h"

namespace vraft {

class ClientThread final {
 public:
  ClientThread(std::string name, bool detach);
  ~ClientThread();
  ClientThread(const ClientThread &t) = delete;
  ClientThread &operator=(const ClientThread &t) = delete;

  // call in any thread
  int32_t Start();
  void Stop();
  void Join();
  void WaitStarted();
  void RunFunctor(Functor func);

  void AddClient(TcpClientSPtr client);
  TcpClientSPtr GetClient(uint64_t dest);
  TcpClientSPtr GetClient(const HostPort &dest);

  // for tcp-client
  void ServerCloseCountDown();
  EventLoopSPtr LoopPtr();

 private:
  void WaitClientClose();

 private:
  std::string name_;
  bool detach_;

  std::mutex mu_;
  std::unordered_map<uint64_t, TcpClientSPtr> clients_;

  LoopThreadSPtr loop_thread_;
  CountDownLatchUPtr stop_;
};

inline ClientThread::ClientThread(std::string name, bool detach)
    : name_(name), detach_(detach) {
  loop_thread_ = std::make_shared<LoopThread>(name_, detach_);
}

inline ClientThread::~ClientThread() {}

}  // namespace vraft

#endif
