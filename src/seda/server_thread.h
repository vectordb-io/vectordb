#ifndef VRAFT_SERVER_THREAD_H_
#define VRAFT_SERVER_THREAD_H_

#include <vector>

#include "count_down.h"
#include "loop_thread.h"
#include "tcp_server.h"

namespace vraft {

struct ServerThreadParam {
  std::string name = "default-server-thread";
  int32_t server_num = 1;
  bool detach = false;
  std::string host = "127.0.0.1";
  uint16_t start_port = 9000;
  TcpOptions options;

  OnMessageCallback on_message_cb;
  OnConnectionCallback on_connection_cb;
  WriteCompleteCallback write_complete_cb;
};

class ServerThread final {
 public:
  ServerThread(const ServerThreadParam &param);
  ~ServerThread();
  ServerThread(const ServerThread &t) = delete;
  ServerThread &operator=(const ServerThread &t) = delete;

  // call in any thread
  int32_t Start();
  void Stop();
  void Join();
  void WaitStarted();
  void RunFunctor(Functor func);

  // call in this thread
  EventLoopSPtr LoopPtr();
  void ServerCloseCountDown();

 private:
  void WaitServerClose();

 private:
  std::string name_;
  int32_t server_num_;
  bool detach_;
  std::string host_;
  uint16_t start_port_;

  OnMessageCallback on_message_cb_;
  OnConnectionCallback on_connection_cb_;
  WriteCompleteCallback write_complete_cb_;
  Functor close_cb_;

 private:
  LoopThreadSPtr loop_thread_;
  std::vector<TcpServerSPtr> servers_;

  CountDownLatchUPtr stop_;
};

inline ServerThread::~ServerThread() {}

class ServerThreadPool final {
 public:
  explicit ServerThreadPool(int32_t thread_num, ServerThreadParam param);
  ~ServerThreadPool();
  ServerThreadPool(const ServerThreadPool &t) = delete;
  ServerThreadPool &operator=(const ServerThreadPool &t) = delete;

  int32_t Start();
  void Stop();
  void Join();
  ServerThreadSPtr GetThread(uint64_t partition_id);
  uint64_t PartitionId(uint64_t id);

 private:
  std::string name_;
  int32_t thread_num_;
  std::unordered_map<uint64_t, ServerThreadSPtr> threads_;  // partition_id
};

}  // namespace vraft

#endif
