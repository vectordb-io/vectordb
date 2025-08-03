#include "loop_thread.h"

namespace vraft {

LoopThread::LoopThread(const std::string& name, bool detach)
    : name_(name), detach_(detach) {
  loop_ = std::make_shared<EventLoop>(name_);
  loop_->Init();
}

LoopThread::~LoopThread() {}

int32_t LoopThread::Start() {
  thread_ = std::thread(std::bind(&LoopThread::Run, this));
  if (detach_) {
    thread_.detach();
  }
  return 0;
}

void LoopThread::Stop() { loop_->Stop(); }

void LoopThread::Join() {
  assert(!detach_);
  thread_.join();
}

void LoopThread::WaitStarted() { loop_->WaitStarted(); }

void LoopThread::RunFunctor(Functor func) { loop_->RunFunctor(func); }

void LoopThread::AddTimer(TimerParam& param) { loop_->AddTimer(param); }

void LoopThread::Run() { loop_->Loop(); }

int32_t LoopThreadPool::Start() {
  for (auto& item : threads_) {
    int32_t rv = item.second->Start();
    assert(rv == 0);
  }
  return 0;
}

void LoopThreadPool::Stop() {
  for (auto& item : threads_) {
    item.second->Stop();
  }
}

void LoopThreadPool::Join() {
  for (auto& item : threads_) {
    item.second->Join();
  }
}

void LoopThreadPool::RunFunctor(uint64_t id, Functor func) {
  uint64_t partition_id = PartitionId(id);
  auto sptr = GetThread(partition_id);
  assert(sptr);
  sptr->RunFunctor(func);
}

LoopThreadSPtr LoopThreadPool::GetThread(uint64_t partition_id) {
  auto it = threads_.find(partition_id);
  if (it == threads_.end()) {
    return nullptr;
  } else {
    return it->second;
  }
}

uint64_t LoopThreadPool::PartitionId(uint64_t id) {
  return id % threads_.size();
}

}  // namespace vraft
