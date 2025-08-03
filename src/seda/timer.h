#ifndef VRAFT_TIMER_H_
#define VRAFT_TIMER_H_

#include <atomic>
#include <map>
#include <memory>

#include "common.h"
#include "uv_wrapper.h"

namespace vraft {

struct TimerParam {
  uint64_t timeout_ms = 0;
  uint64_t repeat_ms = 0;
  TimerFunctor cb;
  void *data = nullptr;
  int64_t repeat_times = 0;
  std::string name;
};
using MakeTimerFunc = std::function<TimerSPtr(TimerParam &param)>;

TimerSPtr CreateTimer(TimerParam &param, EventLoopSPtr &loop);
void HandleUvTimer(UvTimer *uv_timer);
void TimerCloseCb(UvHandle *handle);

class Timer final {
 public:
  Timer(TimerParam &param, EventLoopSPtr &loop);
  ~Timer();
  Timer(const Timer &t) = delete;
  Timer &operator=(const Timer &t) = delete;

  // call in loop thread
  void AssertInLoopThread();
  std::string DebugString();

  // control
  int32_t Start();
  int32_t Stop();
  int32_t Again();
  int32_t Again(uint64_t to_ms, uint64_t rp_ms);

  bool Active();
  void Close();

  // set/get
  EventLoopSPtr LoopSPtr();
  void *UvTimerPtr();
  TimerId id() const;
  void *data();
  void set_data(void *data);
  uint64_t dest_addr();
  void set_dest_addr(uint64_t dest_addr);
  const std::string &name() const;

  // repeat
  int64_t RepeatDecr();
  int64_t repeat_counter() const;
  int64_t repeat_times() const;
  void set_repeat_times(int64_t repeat_times);

 private:
  void Init();

 private:
  std::string name_;
  TimerId id_;

  uint64_t timeout_ms_;
  uint64_t repeat_ms_;
  const TimerFunctor cb_;
  void *data_;

  int64_t repeat_times_;
  int64_t repeat_counter_;
  uint64_t dest_addr_;

  EventLoopWPtr loop_;
  UvTimer uv_timer_;

  // maybe serval threads create Timer object at the same time
  static std::atomic<int64_t> seq_;

  friend void HandleUvTimer(UvTimer *uv_timer);
  friend void TimerCloseCb(UvHandle *handle);
};

inline TimerId Timer::id() const { return id_; }

inline void *Timer::data() { return data_; }

inline void Timer::set_data(void *data) { data_ = data; }

inline uint64_t Timer::dest_addr() { return dest_addr_; }

inline void Timer::set_dest_addr(uint64_t dest_addr) { dest_addr_ = dest_addr; }

inline const std::string &Timer::name() const { return name_; }

inline int64_t Timer::RepeatDecr() { return --repeat_counter_; }

inline int64_t Timer::repeat_counter() const { return repeat_counter_; }

inline int64_t Timer::repeat_times() const { return repeat_times_; }

inline void Timer::set_repeat_times(int64_t repeat_times) {
  repeat_times_ = repeat_times;
  repeat_counter_ = repeat_times;
}

}  // namespace vraft

#endif
