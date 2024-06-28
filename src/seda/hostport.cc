#include "hostport.h"

#include "raft_addr.h"
#include "util.h"

namespace vraft {

uint64_t HostPort::ToU64() const {
  RaftAddr addr(host, port, 0);
  return addr.ToU64();
}

bool ConvertHostPortToSockaddr(const std::string &host, int32_t port,
                               sockaddr &out_sockaddr) {
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;        // IPv4
  hints.ai_socktype = SOCK_STREAM;  // TCP, for example

  // Convert the port number to a string because getaddrinfo expects it as such
  std::string port_str = std::to_string(port);

  // Perform DNS and service name lookup
  int status = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
  if (status != 0) {
    std::cerr << "getaddrinfo failed: " << gai_strerror(status) << '\n';
    return false;
  }

  // Use the first result
  if (res != nullptr) {
    // Assuming the first result is the address we want to use
    memcpy(&out_sockaddr, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);  // Free the linked list allocated by getaddrinfo
    return true;
  }

  return false;
}

bool HostPortToSockaddrIn(const std::string &host, uint16_t port,
                          sockaddr_in &out_sockaddr_in) {
  uint32_t ip32;
  bool b = vraft::HostNameToIpU32(host, ip32);
  if (!b) {
    return b;
  }

  out_sockaddr_in.sin_family = AF_INET;
  out_sockaddr_in.sin_addr.s_addr = htonl(ip32);
  out_sockaddr_in.sin_port = htons(port);

  return true;
}

HostPort SockaddrInToHostPort(sockaddr_in *addr) {
  return HostPort(IpU32ToIpString(ntohl(addr->sin_addr.s_addr)),
                  ntohs(addr->sin_port));
}

}  // namespace vraft
