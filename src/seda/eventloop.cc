#include "eventloop.h"

#include <sys/types.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include "clock.h"
#include "util.h"
#include "vraft_logger.h"

namespace vraft {

void Started(Timer *timer) {
  assert(timer != nullptr);
  timer->AssertInLoopThread();
  auto sptr = timer->LoopSPtr();
  if (sptr) {
    sptr->set_started(true);
    sptr->latch_.CountDown();
    vraft_logger.FInfo("loop started, %s", sptr->DebugString().c_str());
  }
}

EventLoop::EventLoop(const std::string &name)
    : latch_(1), started_(false), name_(name), tid_(0) {
  int32_t rv = UvLoopInit(&uv_loop_);
  vraft_logger.FInfo("loop construct, %s", DebugString().c_str());
  assert(rv == 0);
}

EventLoop::~EventLoop() {
  UvLoopClose(&uv_loop_);
  vraft_logger.FInfo("loop destruct, name:%s, handle:%p", name_.c_str(),
                     &uv_loop_);
}

void EventLoop::Stop() {
  if (IsInLoopThread()) {
    Close();
  } else {
    stop_->Notify();
  }
}

void EventLoop::WaitStarted() {
  latch_.Wait();

#if 0
  while (!started_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
#endif
}

void EventLoop::RunFunctor(const Functor func) {
  if (IsInLoopThread()) {
    func();
  } else {
    functors_->Push(std::move(func));
  }
}

void EventLoop::AssertInLoopThread() const {
  if (!IsInLoopThread()) {
    vraft_logger.FInfo(
        "loop assert, name:%s, handle:%p, tid:%d, current-tid:%d",
        name_.c_str(), &uv_loop_, tid_, gettid());
    assert(0);
  }
}

std::string EventLoop::DebugString() const {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "name:%s, handle:%p, started:%d, active:%d, tid:%d", name_.c_str(),
           &uv_loop_, started_.load(), Alive(), tid_);
  return std::string(buf);
}

int32_t EventLoop::Init() {
  auto sptr = shared_from_this();
  functors_ = std::make_shared<AsyncQueue>(sptr);
  stop_ = std::make_shared<AsyncStop>(sptr);

  TimerParam param;
  param.timeout_ms = 0;
  param.repeat_ms = 0;
  param.name = "start-timer";
  param.cb = Started;
  sptr->AddTimer(param);

  return 0;
}

int32_t EventLoop::Loop() {
  tid_ = gettid();
  vraft_logger.FInfo("loop start, %s", DebugString().c_str());
  return UvRun(&uv_loop_, UV_RUN_DEFAULT);
}

bool EventLoop::Alive() const {
  AssertInLoopThread();
  return UvLoopAlive(&uv_loop_);
}

bool EventLoop::IsInLoopThread() const {
  if (tid_ != 0) {
    return (gettid() == tid_);
  } else {
    return true;
  }
}

TimerSPtr EventLoop::MakeTimer(TimerParam &param) {
  AssertInLoopThread();
  auto sptr = shared_from_this();
  TimerSPtr ptr = CreateTimer(param, sptr);
  return ptr;
}

void EventLoop::AddTimer(TimerParam &param) {
  AssertInLoopThread();
  TimerSPtr timer = MakeTimer(param);
  timer->Start();
  vraft_logger.FInfo("loop %s, add timer, %s", DebugString().c_str(),
                     timer->DebugString().c_str());
  timers_[timer->id()] = std::move(timer);
}

void EventLoop::AddTimer(TimerSPtr timer) {
  AssertInLoopThread();
  timer->Start();
  vraft_logger.FInfo("loop %s, add timer, %s", DebugString().c_str(),
                     timer->DebugString().c_str());
  timers_[timer->id()] = std::move(timer);
}

void EventLoop::RemoveTimer(TimerId id) {
  AssertInLoopThread();
  vraft_logger.FInfo("loop %s, remove timer, timer-id:%d",
                     DebugString().c_str(), id);
  timers_.erase(id);
}

void EventLoop::Close() {
  AssertInLoopThread();
  TimerMap timers2 = timers_;
  for (auto &t : timers2) {
    t.second->Stop();
    t.second->Close();
  }
  stop_->Close();
  functors_->Close();
}

}  // namespace vraft
