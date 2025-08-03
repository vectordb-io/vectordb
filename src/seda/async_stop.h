#ifndef VRAFT_ASYNC_STOP_H_
#define VRAFT_ASYNC_STOP_H_

#include "common.h"
#include "uv_wrapper.h"

namespace vraft {

void StopLoop(UvAsync *uv_async);

class AsyncStop final {
 public:
  AsyncStop(EventLoopSPtr &loop);
  ~AsyncStop();
  AsyncStop(const AsyncStop &a) = delete;
  AsyncStop &operator=(const AsyncStop &a) = delete;

  // call in loop thread
  void AssertInLoopThread();
  void Close();

  // call in any thread
  void Notify();

 private:
  void Init();

 private:
  EventLoopWPtr loop_;
  UvAsync uv_async_;

  friend void StopLoop(UvAsync *uv_async);
  friend void AsyncStopCloseCb(UvHandle *handle);
};

}  // namespace vraft

#endif
