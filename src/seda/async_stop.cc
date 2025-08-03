#include "async_stop.h"

#include "eventloop.h"
#include "vraft_logger.h"

namespace vraft {

void StopLoop(UvAsync *uv_async) {
  AsyncStop *stop = static_cast<AsyncStop *>(uv_async->data);
  stop->AssertInLoopThread();

  auto sptr = stop->loop_.lock();
  if (sptr) {
    sptr->Close();
  }
}

void AsyncStopCloseCb(UvHandle *handle) {
  vraft_logger.FInfo("async_stop:%p close finish", handle);
  AsyncStop *stop = static_cast<AsyncStop *>(handle->data);
  assert(stop != nullptr);

  auto sptr = stop->loop_.lock();
  if (sptr) {
    sptr->ResetAsyncStop();
  }
}

AsyncStop::AsyncStop(EventLoopSPtr &loop) : loop_(loop) { Init(); }

AsyncStop::~AsyncStop() {}

void AsyncStop::AssertInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
  }
}

void AsyncStop::Close() {
  AssertInLoopThread();
  UvClose(reinterpret_cast<uv_handle_t *>(&uv_async_), AsyncStopCloseCb);
}

void AsyncStop::Init() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
    UvAsyncInit(sptr->UvLoopPtr(), &uv_async_, StopLoop);
    uv_async_.data = this;
  }
}

void AsyncStop::Notify() { UvAsyncSend(&uv_async_); }

}  // namespace vraft
