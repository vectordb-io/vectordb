#ifndef VRAFT_RAFT_ADDR_H_
#define VRAFT_RAFT_ADDR_H_

#include <cstdint>
#include <string>

#include "coding.h"
#include "util.h"

namespace vraft {

class RaftAddr final {
 public:
  RaftAddr(uint32_t ip, uint16_t port, int16_t id);
  RaftAddr(std::string ip_str, uint16_t port, int16_t id);
  explicit RaftAddr(uint64_t u64 = 0);
  ~RaftAddr();

  RaftAddr(const RaftAddr &t) = default;
  RaftAddr &operator=(const RaftAddr &t) = default;

  bool operator==(const RaftAddr &rhs) const;
  bool operator<(const RaftAddr &rhs) const;

  bool FromString(const std::string s);  // 127.0.0.1:9988#0
  uint64_t ToU64() const;
  void FromU64(uint64_t u64);
  std::string ToString() const;

  uint32_t ip() const { return ip_; }
  uint16_t port() const { return port_; }
  int16_t id() const { return id_; }

 private:
  void FromU64(uint64_t u64, uint32_t &ip, uint16_t &port, int16_t &id);

 private:
  uint32_t ip_;
  uint16_t port_;
  int16_t id_;
};

inline RaftAddr::RaftAddr(uint32_t ip, uint16_t port, int16_t id)
    : ip_(ip), port_(port), id_(id) {}

inline RaftAddr::RaftAddr(std::string ip_str, uint16_t port, int16_t id)
    : port_(port), id_(id) {
  bool b = StringToIpU32(ip_str, ip_);
  assert(b);
}

inline RaftAddr::RaftAddr(uint64_t u64) { FromU64(u64, ip_, port_, id_); }

inline bool RaftAddr::operator==(const RaftAddr &rhs) const {
  return (ip_ == rhs.ip_ && port_ == rhs.port_ && id_ == rhs.id_);
}

inline bool RaftAddr::operator<(const RaftAddr &rhs) const {
  return (ToU64() < rhs.ToU64());
}

// 127.0.0.1:9988#0
inline bool RaftAddr::FromString(const std::string s) {
  std::vector<std::string> result;
  Split(s, '#', result);
  assert(result.size() == 2);

  // id
  sscanf(result[1].c_str(), "%hd", &id_);

  std::vector<std::string> ipport;
  Split(result[0], ':', ipport);

  // port
  sscanf(ipport[1].c_str(), "%hu", &port_);

  // ip32
  bool b = HostNameToIpU32(ipport[0], ip_);
  return b;
}

inline RaftAddr::~RaftAddr() {}

inline uint64_t RaftAddr::ToU64() const {
  uint64_t u64 = 0;
  char *p = reinterpret_cast<char *>(&u64);

  EncodeFixed32(p, ip_);
  p += sizeof(ip_);

  EncodeFixed16(p, port_);
  p += sizeof(port_);

  EncodeFixed16(p, id_);
  p += sizeof(id_);

  return u64;
}

inline void RaftAddr::FromU64(uint64_t u64) { FromU64(u64, ip_, port_, id_); }

inline void RaftAddr::FromU64(uint64_t u64, uint32_t &ip, uint16_t &port,
                              int16_t &id) {
  char *p = reinterpret_cast<char *>(&u64);

  ip_ = DecodeFixed32(p);
  p += sizeof(ip_);

  port_ = DecodeFixed16(p);
  p += sizeof(port_);

  id_ = DecodeFixed16(p);
  p += sizeof(id_);
}

inline std::string RaftAddr::ToString() const {
  std::string ip_str = IpU32ToIpString(ip_);
  char buf[128];
  snprintf(buf, sizeof(buf), "%s:%d#%d", ip_str.c_str(), port_, id_);
  return std::string(buf);
}

}  // namespace vraft

#endif
