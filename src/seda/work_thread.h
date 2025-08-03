#ifndef VRAFT_WORK_THREAD_H_
#define VRAFT_WORK_THREAD_H_

#include <sys/types.h>

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common.h"

namespace vraft {

class WorkThread final {
 public:
  explicit WorkThread(const std::string &name, int32_t max_queue_size,
                      bool detach);
  explicit WorkThread(const std::string &name);
  ~WorkThread();
  WorkThread(const WorkThread &t) = delete;
  WorkThread &operator=(const WorkThread &t) = delete;

  // call in any thread
  int32_t Start();
  void Stop();
  void Join();
  void Push(Functor func);

  // call in this thread
  int32_t max_queue_size();

 private:
  void Run();

 private:
  std::string name_;
  int32_t max_queue_size_;
  bool started_;
  int32_t tid_;
  bool detach_;

  std::mutex mu_;
  std::condition_variable producer_cv_;
  std::condition_variable consumer_cv_;

  std::deque<Functor> functors_;
  std::thread thread_;
};

inline WorkThread::WorkThread(const std::string &name, int32_t max_queue_size,
                              bool detach)
    : name_(name),
      max_queue_size_(max_queue_size),
      started_(false),
      tid_(0),
      detach_(detach) {}

inline WorkThread::WorkThread(const std::string &name)
    : name_(name),
      max_queue_size_(MAX_QUEUE_SIZE),
      started_(false),
      tid_(0),
      detach_(false) {}

inline WorkThread::~WorkThread() { functors_.clear(); }

inline int32_t WorkThread::max_queue_size() { return max_queue_size_; }

class WorkThreadPool final {
 public:
  explicit WorkThreadPool(const std::string &name, int32_t thread_num,
                          int32_t max_queue_size = MAX_QUEUE_SIZE);
  ~WorkThreadPool();
  WorkThreadPool(const WorkThreadPool &t) = delete;
  WorkThreadPool &operator=(const WorkThreadPool &t) = delete;

  int32_t Start();
  void Stop();
  void Join();
  void Push(uint64_t id, Functor func);
  WorkThreadSPtr GetThread(uint64_t partition_id);
  uint64_t PartitionId(uint64_t id);

 private:
  std::string name_;
  int32_t thread_num_;
  std::unordered_map<uint64_t, WorkThreadSPtr> threads_;  // partition_id
};

inline WorkThreadPool::WorkThreadPool(const std::string &name,
                                      int32_t thread_num,
                                      int32_t max_queue_size)
    : name_(name), thread_num_(thread_num) {
  for (int32_t i = 0; i < thread_num; ++i) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s_%d", name_.c_str(), i);
    WorkThreadSPtr sptr =
        std::make_shared<WorkThread>(buf, max_queue_size, false);
    threads_[static_cast<uint64_t>(i)] = sptr;
  }
}

inline WorkThreadPool::~WorkThreadPool() {}

}  // namespace vraft

#endif
