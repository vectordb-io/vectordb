#include "timer.h"

#include <cassert>

#include "eventloop.h"
#include "vraft_logger.h"

namespace vraft {

TimerSPtr CreateTimer(TimerParam &param, EventLoopSPtr &loop) {
  TimerSPtr ptr = std::make_shared<Timer>(param, loop);
  return ptr;
}

void HandleUvTimer(UvTimer *uv_timer) {
  Timer *timer = reinterpret_cast<Timer *>(uv_timer->data);
  assert(timer != nullptr);
  timer->AssertInLoopThread();

  if (timer->cb_) {
    timer->cb_(timer);
  }
}

void TimerCloseCb(UvHandle *handle) {
  vraft_logger.FInfo("timer:%p close finish", handle);
  Timer *timer = reinterpret_cast<Timer *>(handle->data);
  auto sptr = timer->loop_.lock();
  if (sptr) {
    sptr->RemoveTimer(timer->id());
  }
}

Timer::Timer(TimerParam &param, EventLoopSPtr &loop)
    : name_(param.name),
      id_(seq_.fetch_add(1)),
      timeout_ms_(param.timeout_ms),
      repeat_ms_(param.repeat_ms),
      cb_(param.cb),
      data_(param.data),
      repeat_times_(param.repeat_times),
      repeat_counter_(param.repeat_times),
      dest_addr_(0),
      loop_(loop) {
  vraft_logger.FInfo("timer construct, %s", DebugString().c_str());
  Init();
}

Timer::~Timer() {
  vraft_logger.FInfo("timer destruct, %s", DebugString().c_str());
}

void Timer::AssertInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
  }
}

std::string Timer::DebugString() {
  void *lptr = nullptr;
  auto sptr = loop_.lock();
  if (sptr) {
    lptr = sptr->UvLoopPtr();
  }
  char buf[256];
  snprintf(buf, sizeof(buf),
           "id:%ld, name:%s, timeout:%lu, repeat:%lu, handle:%p, loop:%p", id_,
           name_.c_str(), timeout_ms_, repeat_ms_, &uv_timer_, lptr);
  return std::string(buf);
}

int32_t Timer::Start() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
    int32_t rv =
        UvimerStart(&uv_timer_, HandleUvTimer, timeout_ms_, repeat_ms_);
    assert(rv == 0);
    return rv;

  } else {
    return -1;
  }
}

int32_t Timer::Stop() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();

    int32_t rv = 0;
    if (!UvIsClosing(reinterpret_cast<UvHandle *>(&uv_timer_))) {
      rv = UvTimerStop(&uv_timer_);
      assert(rv == 0);

    } else {
      vraft_logger.FInfo("timer stop, already closing, %s",
                         DebugString().c_str());
      rv = -1;
    }
    return rv;

  } else {
    return -1;
  }
}

int32_t Timer::Again() {
  AssertInLoopThread();
  int32_t rv = UvTimerAgain(&uv_timer_);
  assert(rv == 0);
  return rv;
}

int32_t Timer::Again(uint64_t to_ms, uint64_t rp_ms) {
  AssertInLoopThread();
  timeout_ms_ = to_ms;
  repeat_ms_ = rp_ms;

  int32_t rv = UvTimerStop(&uv_timer_);
  assert(rv == 0);

  rv = UvimerStart(&uv_timer_, HandleUvTimer, timeout_ms_, repeat_ms_);
  assert(rv == 0);
  return rv;
}

bool Timer::Active() {
  AssertInLoopThread();
  return UvIsActive(reinterpret_cast<UvHandle *>(&uv_timer_));
}

void Timer::Close() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();

    if (!UvIsClosing(reinterpret_cast<UvHandle *>(&uv_timer_))) {
      UvClose(reinterpret_cast<uv_handle_t *>(&uv_timer_), TimerCloseCb);

    } else {
      vraft_logger.FInfo("timer close, already closing, %s",
                         DebugString().c_str());
    }
  }
}

EventLoopSPtr Timer::LoopSPtr() {
  auto sptr = loop_.lock();
  return sptr;
}

// for debug
void *Timer::UvTimerPtr() { return &uv_timer_; }

void Timer::Init() {
  auto sptr = loop_.lock();
  if (sptr) {
    UvTimerInit(sptr->UvLoopPtr(), &uv_timer_);
    uv_timer_.data = this;
  }
}

std::atomic<int64_t> Timer::seq_(0);

}  // namespace vraft
