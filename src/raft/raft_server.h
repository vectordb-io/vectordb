#ifndef VRAFT_RAFT_SERVER_H_
#define VRAFT_RAFT_SERVER_H_

#include <memory>
#include <unordered_map>

#include "allocator.h"
#include "common.h"
#include "config.h"
#include "raft.h"
#include "tcp_client.h"
#include "tcp_server.h"

namespace vraft {

class RaftServer final {
 public:
  RaftServer(EventLoopSPtr &loop, Config &config);
  ~RaftServer();
  RaftServer(const RaftServer &t) = delete;
  RaftServer &operator=(const RaftServer &t) = delete;

  // call back
  void OnConnection(const vraft::TcpConnectionSPtr &conn);
  void OnMessage(const vraft::TcpConnectionSPtr &conn, vraft::Buffer *buf);

  // control
  int32_t Start();
  int32_t Stop();

  // get
  RaftSPtr raft();
  Config &config();
  EventLoopSPtr LoopSPtr();

  // debug
  void Print(bool tiny, bool one_line);
  void AssertInLoopThread();

 private:
  void Init();
  int32_t Send(uint64_t dest_addr, const char *buf, unsigned int size);

  TimerSPtr MakeTimer(TimerParam &param);
  TcpClientSPtr GetClient(uint64_t dest_addr);
  TcpClientSPtr GetClientOrCreate(uint64_t dest_addr);

 private:
  Config config_;
  EventLoopWPtr loop_;
  TcpServerSPtr server_;
  std::unordered_map<uint64_t, TcpClientSPtr> clients_;

  RaftSPtr raft_;
};

inline Config &RaftServer::config() { return config_; }

inline EventLoopSPtr RaftServer::LoopSPtr() {
  auto sptr = loop_.lock();
  return sptr;
}

inline RaftSPtr RaftServer::raft() { return raft_; }

inline void RaftServer::Print(bool tiny, bool one_line) {
  raft_->Print(tiny, one_line);
}

}  // namespace vraft

#endif
