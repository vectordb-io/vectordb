#include "client_thread.h"

#include "raft_addr.h"

namespace vraft {

int32_t ClientThread::Start() {
  int32_t rv = 0;
  for (auto &item : clients_) {
    rv = item.second->Connect();
    assert(rv == 0);
  }

  stop_ =
      std::make_unique<CountDownLatch>(static_cast<int32_t>(clients_.size()));

  rv = loop_thread_->Start();
  assert(rv == 0);

  return 0;
}

void ClientThread::Stop() {
  for (auto &item : clients_) {
    item.second->Stop();
  }

  WaitClientClose();
  loop_thread_->Stop();
}

void ClientThread::Join() {
  assert(!detach_);
  loop_thread_->Join();
}

void ClientThread::WaitStarted() { loop_thread_->WaitStarted(); }

void ClientThread::RunFunctor(Functor func) { loop_thread_->RunFunctor(func); }

void ClientThread::AddClient(TcpClientSPtr client) {
  std::unique_lock<std::mutex> ulk(mu_);
  clients_[client->dest_addr().ToU64()] = client;
}

TcpClientSPtr ClientThread::GetClient(uint64_t dest) {
  TcpClientSPtr sptr;
  {
    std::unique_lock<std::mutex> ulk(mu_);
    auto it = clients_.find(dest);
    if (it != clients_.end()) {
      sptr = it->second;
    }
  }
  return sptr;
}

TcpClientSPtr ClientThread::GetClient(const HostPort &dest) {
  return GetClient(dest.ToU64());
}

void ClientThread::ServerCloseCountDown() { stop_->CountDown(); }

EventLoopSPtr ClientThread::LoopPtr() { return loop_thread_->loop(); }

void ClientThread::WaitClientClose() { stop_->Wait(); }

}  // namespace vraft
