#include "async_queue.h"

#include "eventloop.h"

namespace vraft {

void AsyncQueueCb(UvAsync *uv_async) {
  AsyncQueue *async_queue = static_cast<AsyncQueue *>(uv_async->data);
  assert(async_queue != nullptr);
  async_queue->DoFunctor();
}

void AsyncQueueCloseCb(UvHandle *handle) {
  vraft_logger.FInfo("async_queue:%p close finish", handle);
  AsyncQueue *async_queue = static_cast<AsyncQueue *>(handle->data);
  assert(async_queue != nullptr);

  auto sptr = async_queue->loop_.lock();
  if (sptr) {
    sptr->ResetAsyncQueue();
  }
}

AsyncQueue::AsyncQueue(EventLoopSPtr &loop) : loop_(loop) { Init(); }

AsyncQueue::~AsyncQueue() {}

void AsyncQueue::AssertInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
  }
}

void AsyncQueue::Close() {
  AssertInLoopThread();
  UvClose(reinterpret_cast<uv_handle_t *>(&uv_async_), AsyncQueueCloseCb);
}

void AsyncQueue::Push(const Functor func) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    functors_.push(func);
  }
  UvAsyncSend(&uv_async_);
}

void AsyncQueue::Init() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
    UvAsyncInit(sptr->UvLoopPtr(), &uv_async_, AsyncQueueCb);
    uv_async_.data = this;
  }
}

void AsyncQueue::DoFunctor() {
  AssertInLoopThread();

  std::queue<Functor> todo_queue;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    todo_queue.swap(functors_);
  }
  while (!todo_queue.empty()) {
    auto func = todo_queue.front();
    func();
    todo_queue.pop();
  }
}

}  // namespace vraft
