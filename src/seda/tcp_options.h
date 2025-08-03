#ifndef VRAFT_TCP_OPTIONS_H_
#define VRAFT_TCP_OPTIONS_H_

#include <cstdint>

namespace vraft {

struct TcpOptions {
  bool tcp_nodelay = true;
  int32_t retry_interval_ms = 1000;
};

}  // namespace vraft

#endif