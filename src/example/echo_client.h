#ifndef VRAFT_ECHO_SERVER_H_
#define VRAFT_ECHO_SERVER_H_

#include <csignal>
#include <iostream>

#include "clock.h"
#include "config.h"
#include "eventloop.h"
#include "hostport.h"
#include "tcp_client.h"
#include "tcp_connection.h"
#include "vraft_logger.h"

inline void Ping(vraft::TcpConnectionSPtr &conn, vraft::Timer *t) {
  char buf[128];
  snprintf(buf, sizeof(buf), "ping %ld", vraft::Clock::Sec());
  conn->BufSend(buf, strlen(buf) + 1);
  vraft::vraft_logger.FInfo("echo-client send msg:[%s]", buf);
}

inline void OnConnection(const vraft::TcpConnectionSPtr &conn) {
  vraft::vraft_logger.FInfo("echo-client OnConnection:%s",
                            conn->name().c_str());

  auto sptr = conn->LoopSPtr();
  if (sptr) {
    vraft::TimerParam param;
    param.timeout_ms = 0;
    param.repeat_ms = 1000;
    param.cb = std::bind(Ping, conn, std::placeholders::_1);
    param.data = nullptr;
    sptr->AddTimer(param);
  }
}

inline void OnMessage(const vraft::TcpConnectionSPtr &conn,
                      vraft::Buffer *buf) {
  std::string s(buf->BeginRead(), buf->ReadableBytes());
  buf->RetrieveAll();

  // do not send
  // conn->CopySend(s.c_str(), s.size());

  // delete 0xD 0xA, for print pretty
  if (s.size() >= 2) {
    if (s[s.length() - 1] == 0xA && s[s.length() - 2] == 0xD) {
      s.erase(s.size() - 2);
    }
  }

  vraft::vraft_logger.FInfo("echo-client OnMessage:[%s]", s.c_str());
  std::cout << "echo-client recv " << s << std::endl;
}

#endif