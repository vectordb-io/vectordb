#ifndef VRAFT_HOSTPORT_H_
#define VRAFT_HOSTPORT_H_

#include <arpa/inet.h>
#include <netdb.h>       // getaddrinfo, freeaddrinfo
#include <netinet/in.h>  // sockaddr_in
#include <sys/socket.h>
#include <sys/types.h>

#include <cassert>
#include <cstdint>
#include <cstring>  // memset
#include <iostream>
#include <string>

#include "util.h"

/**********************

struct sockaddr {
    unsigned short sa_family; // address family, AF_xxx
    char sa_data[14]; // 14 bytes of protocol address
};

struct sockaddr_in {
    short            sin_family;   // 通常为 AF_INET
    unsigned short   sin_port;     // 端口号，网络字节序
    struct in_addr   sin_addr;     // IPv4 地址
    unsigned char    sin_zero[8];  // 用于填充，使结构体和 struct sockaddr
大小相同
};

struct in_addr {
    uint32_t s_addr; // 该成员是一个无符号整型，存储网络字节序的IPv4地址
};

struct sockaddr_in6 {
    sa_family_t      sin6_family;   // 通常为 AF_INET6
    in_port_t        sin6_port;     // 端口号，网络字节序
    uint32_t         sin6_flowinfo; // 流信息
    struct in6_addr  sin6_addr;     // IPv6 地址
    uint32_t         sin6_scope_id; // 接口索引
};

struct in6_addr {
    unsigned char s6_addr[16]; // IPv6地址，128位
};

**********************/

namespace vraft {

// port是主机字节序，小端
// out_sockaddr_in的内容是网络字节序，大端
bool HostPortToSockaddrIn(const std::string &host, uint16_t port,
                          sockaddr_in &out_sockaddr_in);

// addr的内容是网络字节序，大端
// 返回值HostPort是主机字节序，小端
class HostPort;
HostPort SockaddrInToHostPort(sockaddr_in *addr);

class HostPort final {
 public:
  HostPort(const std::string &hostport);
  HostPort(const std::string &h, uint16_t p);
  HostPort();
  ~HostPort();
  HostPort(const HostPort &t) = default;
  HostPort &operator=(const HostPort &t) = default;

  const std::string ToString() const;
  uint64_t ToU64() const;

 public:
  std::string host;
  uint16_t port;

  bool convert_ok;
  sockaddr addr;
  uint32_t ip32;

 private:
  void Init();
};

inline void HostPort::Init() {
  sockaddr_in addr_in;
  convert_ok = HostPortToSockaddrIn(host, port, addr_in);
  assert(convert_ok);
  assert(sizeof(addr) == sizeof(addr_in));
  memcpy(&addr, &addr_in, sizeof(addr_in));

  convert_ok = StringToIpU32(host, ip32);
  assert(convert_ok);
}

inline HostPort::HostPort() : host(""), port(0), convert_ok(false) {}

inline HostPort::HostPort(const std::string &h, uint16_t p)
    : host(h), port(p), convert_ok(false) {
  Init();
}

inline HostPort::HostPort(const std::string &hostport) {
  std::vector<std::string> vec;
  Split(hostport, ':', vec);
  assert(vec.size() == 2);
  host = vec[0];
  sscanf(vec[1].c_str(), "%hu", &port);
  Init();
}

inline HostPort::~HostPort() {}

inline const std::string HostPort::ToString() const {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s:%hu", host.c_str(), port);
  return std::string(buf);
}

}  // namespace vraft

#endif
