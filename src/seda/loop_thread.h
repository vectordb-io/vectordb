#ifndef VRAFT_LOOP_THREAD_H_
#define VRAFT_LOOP_THREAD_H_

#include <memory>
#include <thread>
#include <unordered_map>

#include "common.h"
#include "eventloop.h"

namespace vraft {

class LoopThread final {
 public:
  explicit LoopThread(const std::string &name, bool detach);
  ~LoopThread();
  LoopThread(const LoopThread &lt) = delete;
  LoopThread &operator=(const LoopThread &lt) = delete;

  // call in any thread
  int32_t Start();
  void Stop();
  void Join();
  void WaitStarted();
  void RunFunctor(Functor func);
  void AddTimer(TimerParam &param);

  // call in this thread
  EventLoopSPtr loop();

 private:
  void Run();

 private:
  std::string name_;
  bool detach_;
  std::thread thread_;
  EventLoopSPtr loop_;
};

inline EventLoopSPtr LoopThread::loop() { return loop_; }

class LoopThreadPool final {
 public:
  explicit LoopThreadPool(const std::string &name, int32_t thread_num);
  ~LoopThreadPool();
  LoopThreadPool(const LoopThreadPool &t) = delete;
  LoopThreadPool &operator=(const LoopThreadPool &t) = delete;

  int32_t Start();
  void Stop();
  void Join();
  void RunFunctor(uint64_t id, Functor func);
  LoopThreadSPtr GetThread(uint64_t partition_id);
  uint64_t PartitionId(uint64_t id);

 private:
  std::string name_;
  int32_t thread_num_;
  std::unordered_map<uint64_t, LoopThreadSPtr> threads_;  // partition_id
};

inline LoopThreadPool::LoopThreadPool(const std::string &name,
                                      int32_t thread_num)
    : name_(name), thread_num_(thread_num) {
  for (int32_t i = 0; i < thread_num; ++i) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s_%d", name_.c_str(), i);
    LoopThreadSPtr sptr = std::make_shared<LoopThread>(buf, false);
    threads_[static_cast<uint64_t>(i)] = sptr;
  }
}

inline LoopThreadPool::~LoopThreadPool() {}

}  // namespace vraft

#endif
