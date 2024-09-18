#ifndef VRAFT_TURING_MACHINE_H_
#define VRAFT_TURING_MACHINE_H_

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "eventloop.h"

namespace vraft {

#define INIT_POS 20

char characters[] = {'0', '1'};

enum HeadState {
  kHeadState0 = 0,
  kHeadState1,
  kHeadState2,
};

inline std::string HeadStateToString(HeadState s) {
  switch (s) {
    case kHeadState0:
      return "State0";
    case kHeadState1:
      return "State1";
    case kHeadState2:
      return "State2";

    default:
      assert(0);
  }
}

enum HeadDirection {
  kDirectionLeft,
  kDirectionRight,
  kDirectionHalt,
};

inline std::string HeadDirectionToString(HeadDirection d) {
  switch (d) {
    case kDirectionLeft:
      return "Left";
    case kDirectionRight:
      return "Right";
    case kDirectionHalt:
      return "Halt";

    default:
      assert(0);
  }
}

class Head final {
 public:
  explicit Head(HeadState state, int32_t pos);
  ~Head();
  Head(const Head &) = delete;
  Head &operator=(const Head &) = delete;

  char Read();
  void Write(char c);
  void MoveLeft();
  void MoveRight();

  HeadState state() const;
  int32_t pos() const;
  std::vector<char> &tape();
  void set_state(HeadState state);
  std::string ToString();

 private:
  HeadState state_;
  int32_t pos_;
  std::vector<char> tape_;
};

inline Head::Head(HeadState state, int32_t pos)
    : state_(state), pos_(pos), tape_(INIT_POS * 2, '0') {}

inline Head::~Head() {}

inline HeadState Head::state() const { return state_; }

inline int32_t Head::pos() const { return pos_; }

inline std::vector<char> &Head::tape() { return tape_; }

inline void Head::set_state(HeadState state) { state_ = state; }

struct RuleKey {
  HeadState current_state;
  char read_char;

  bool operator<(const RuleKey &rhs) const {
    if (current_state < rhs.current_state) {
      return true;
    } else {
      return read_char < rhs.read_char;
    }
  }
};

struct RuleValue {
  char write_char;
  HeadDirection direction;
  HeadState next_state;
};

void timer_cb(Timer *timer);

class TuringMachine final {
 public:
  explicit TuringMachine();
  ~TuringMachine();
  TuringMachine(const TuringMachine &) = delete;
  TuringMachine &operator=(const TuringMachine &) = delete;

  void Run();
  void DoEvent();
  void Print();

 private:
  void InitRules();

 private:
  std::map<RuleKey, RuleValue> rules_;
  Head head_;
  EventLoopSPtr loop_;
  int32_t step_;
};

inline TuringMachine::~TuringMachine() {}

}  // namespace vraft

#endif
