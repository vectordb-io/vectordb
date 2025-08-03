#ifndef VRAFT_ASYNC_QUEUE_H_
#define VRAFT_ASYNC_QUEUE_H_

#include <functional>
#include <memory>
#include <mutex>
#include <queue>

#include "common.h"
#include "uv_wrapper.h"

namespace vraft {

void AsyncQueueCb(UvAsync *uv_async);
void AsyncQueueCloseCb(UvHandle *handle);

class AsyncQueue {
 public:
  AsyncQueue(EventLoopSPtr &loop);
  ~AsyncQueue();
  AsyncQueue(const AsyncQueue &a) = delete;
  AsyncQueue &operator=(const AsyncQueue &a) = delete;

  // call in loop thread
  void AssertInLoopThread();
  void Close();

  // call in any thread
  void Push(const Functor func);

 private:
  void Init();
  void DoFunctor();

 private:
  EventLoopWPtr loop_;
  UvAsync uv_async_;

  std::mutex mutex_;
  std::queue<Functor> functors_;  // guarded by mutex_

  friend void AsyncQueueCb(UvAsync *uv_async);
  friend void AsyncQueueCloseCb(UvHandle *handle);
};

}  // namespace vraft

#endif
