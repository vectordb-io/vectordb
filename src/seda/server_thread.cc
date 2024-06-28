#include "server_thread.h"

namespace vraft {

ServerThread::ServerThread(const ServerThreadParam &param)
    : name_(param.name),
      server_num_(param.server_num),
      detach_(param.detach),
      host_(param.host),
      start_port_(param.start_port) {
  loop_thread_ = std::make_shared<LoopThread>(name_, detach_);

  for (int32_t i = 0; i < server_num_; ++i) {
    char name_buf[128];
    snprintf(name_buf, sizeof(name_buf), "%s-%d", name_.c_str(), i);

    auto sptr = loop_thread_->loop();
    TcpServerSPtr tcp_server = std::make_shared<vraft::TcpServer>(
        sptr, name_buf, vraft::HostPort(host_, start_port_ + i), param.options);
    tcp_server->set_on_connection_cb(param.on_connection_cb);
    tcp_server->set_on_message_cb(param.on_message_cb);
    tcp_server->set_close_cb(
        std::bind(&ServerThread::ServerCloseCountDown, this));

    servers_.push_back(tcp_server);
  }

  stop_ =
      std::make_unique<CountDownLatch>(static_cast<int32_t>(servers_.size()));
}

int32_t ServerThread::Start() {
  int32_t rv = 0;
  for (auto &sptr : servers_) {
    rv = sptr->Start();
    assert(rv == 0);
  }

  rv = loop_thread_->Start();
  assert(rv == 0);

  return 0;
}

void ServerThread::Stop() {
  for (auto &sptr : servers_) {
    sptr->Stop();
  }

  WaitServerClose();
  loop_thread_->Stop();
}

void ServerThread::Join() {
  assert(!detach_);
  loop_thread_->Join();
}

void ServerThread::WaitStarted() { loop_thread_->WaitStarted(); }

void ServerThread::RunFunctor(Functor func) { loop_thread_->RunFunctor(func); }

EventLoopSPtr ServerThread::LoopPtr() { return loop_thread_->loop(); }

void ServerThread::ServerCloseCountDown() { stop_->CountDown(); }

void ServerThread::WaitServerClose() { stop_->Wait(); }

ServerThreadPool::ServerThreadPool(int32_t thread_num, ServerThreadParam param)
    : name_(param.name), thread_num_(thread_num) {
  for (int32_t i = 0; i < thread_num_; ++i) {
    char name_buf[128];
    snprintf(name_buf, sizeof(name_buf), "%s_%d", name_.c_str(), i);
    param.name = name_buf;
    ServerThreadSPtr sptr = std::make_shared<ServerThread>(param);
    threads_[static_cast<uint64_t>(i)] = sptr;
    param.start_port += param.server_num;
  }
}

ServerThreadPool::~ServerThreadPool() {}

int32_t ServerThreadPool::Start() {
  for (auto &item : threads_) {
    int32_t rv = item.second->Start();
    assert(rv == 0);
  }
  return 0;
}

void ServerThreadPool::Stop() {
  for (auto &item : threads_) {
    item.second->Stop();
  }
}

void ServerThreadPool::Join() {
  for (auto &item : threads_) {
    item.second->Join();
  }
}

ServerThreadSPtr ServerThreadPool::GetThread(uint64_t partition_id) {
  auto it = threads_.find(partition_id);
  if (it == threads_.end()) {
    return nullptr;
  } else {
    return it->second;
  }
}

uint64_t ServerThreadPool::PartitionId(uint64_t id) {
  return id % threads_.size();
}

}  // namespace vraft
