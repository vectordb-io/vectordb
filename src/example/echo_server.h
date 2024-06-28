#ifndef VRAFT_ECHO_SERVER_H_
#define VRAFT_ECHO_SERVER_H_

#include <iostream>
#include <memory>
#include <vector>

#include "config.h"
#include "eventloop.h"
#include "hostport.h"
#include "logger.h"
#include "server_thread.h"
#include "tcp_connection.h"
#include "tcp_server.h"
#include "vraft_logger.h"

class EchoServer;
using EchoServerSPtr = std::shared_ptr<EchoServer>;
using EchoServerWPtr = std::weak_ptr<EchoServer>;

class EchoServer {
 public:
  EchoServer(vraft::HostPort listen_addr, vraft::TcpOptions &options,
             int32_t server_num, int32_t thread_num)
      : thread_pool_(
            thread_num,
            vraft::ServerThreadParam{
                "echo-server-thread", server_num, false, listen_addr.host,
                listen_addr.port, options,
                std::bind(&EchoServer::OnMessage, this, std::placeholders::_1,
                          std::placeholders::_2),
                std::bind(&EchoServer::OnConnection, this,
                          std::placeholders::_1),
                nullptr}) {}

  void Start() { thread_pool_.Start(); }
  void Join() { thread_pool_.Join(); }
  void Stop() { thread_pool_.Stop(); }

 private:
  void OnConnection(const vraft::TcpConnectionSPtr &conn) {
    vraft::vraft_logger.FInfo("echo-server on-connection, %s",
                              conn->DebugString().c_str());
  }

  void OnMessage(const vraft::TcpConnectionSPtr &conn, vraft::Buffer *buf) {
    std::string s(buf->BeginRead(), buf->ReadableBytes());
    buf->RetrieveAll();
    conn->CopySend(s.c_str(), s.size());

    // delete 0xD 0xA, for print pretty
    if (s.size() >= 2) {
      if (s[s.length() - 1] == 0xA && s[s.length() - 2] == 0xD) {
        s.erase(s.size() - 2);
      }
    }

    vraft::vraft_logger.FInfo("connection:%s on-message:[%s]",
                              conn->name().c_str(), s.c_str());
    std::cout << "connection:" << conn->name() << " on-message: " << s
              << std::endl;
  }

  vraft::ServerThreadPool thread_pool_;
};

#endif