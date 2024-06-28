#include "tcp_server.h"

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "eventloop.h"
#include "vraft_logger.h"

namespace vraft {

TcpServer::TcpServer(EventLoopSPtr &loop, const std::string &name,
                     const HostPort &listen_addr, const TcpOptions &options)
    : name_(name),
      loop_(loop),
      connections_(),
      acceptor_(loop, listen_addr, options) {
  Init();
  vraft_logger.FInfo("tcp-server construct, %s", DebugString().c_str());
}

TcpServer::~TcpServer() {
  vraft_logger.FInfo("tcp-server destruct, %s", DebugString().c_str());
}

void TcpServer::Stop() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->RunFunctor(std::bind(&TcpServer::Close, this));
  }
}

void TcpServer::RunFunctor(const Functor func) {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->RunFunctor(func);
  }
}

void TcpServer::AssertInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
  }
}

std::string TcpServer::DebugString() const {
  void *lptr = nullptr;
  auto sptr = loop_.lock();
  if (sptr) {
    lptr = sptr->UvLoopPtr();
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "name:%s, loop:%p, acceptor:%s, conn:%lu",
           name_.c_str(), lptr, acceptor_.DebugString().c_str(),
           connections_.size());
  return std::string(buf);
}

int32_t TcpServer::Start() {
  AssertInLoopThread();
  vraft_logger.FInfo("tcp-server start, %s", DebugString().c_str());
  acceptor_.set_close_cb(close_cb_);
  int32_t rv = acceptor_.Start();
  assert(rv == 0);
  return rv;
}

void TcpServer::AddConnection(TcpConnectionSPtr &conn) {
  AssertInLoopThread();
  vraft_logger.FInfo("tcp-server %s, add connection, %s", DebugString().c_str(),
                     conn->DebugString().c_str());
  connections_[conn->name()] = conn;
  int32_t rv = conn->Start();
  assert(rv == 0);
}

void TcpServer::RemoveConnection(const TcpConnectionSPtr &conn) {
  AssertInLoopThread();
  vraft_logger.FInfo("tcp-server %s, remove connection, %s",
                     DebugString().c_str(), conn->DebugString().c_str());
  connections_.erase(conn->name());
}

void TcpServer::Init() {
  acceptor_.set_new_conn_func(
      std::bind(&TcpServer::NewConnection, this, std::placeholders::_1));
}

void TcpServer::NewConnection(UvTcpUPtr client) {
  AssertInLoopThread();

  sockaddr_in local_addr, peer_addr;
  int namelen = sizeof(sockaddr_in);
  int rv = 0;

  rv = UvTcpGetSockName(client.get(), (struct sockaddr *)(&local_addr),
                        &namelen);
  assert(rv == 0);
  HostPort local_hp = SockaddrInToHostPort(&local_addr);

  rv =
      UvTcpGetPeerName(client.get(), (struct sockaddr *)(&peer_addr), &namelen);
  assert(rv == 0);
  HostPort peer_hp = SockaddrInToHostPort(&peer_addr);

  std::string conn_name =
      name_ + "#" + local_hp.ToString() + "#" + peer_hp.ToString();

  auto sptr = loop_.lock();
  if (sptr) {
    TcpConnectionSPtr conn =
        std::make_shared<TcpConnection>(sptr, conn_name, std::move(client));

    conn->set_on_connection_cb(on_connection_cb_);
    conn->set_on_message_cb(on_message_cb_);
    conn->set_write_complete_cb(write_complete_cb_);
    conn->set_connection_close_cb(
        std::bind(&TcpServer::RemoveConnection, this, std::placeholders::_1));
    vraft_logger.FInfo("tcp-server %s, new connection, %s",
                       DebugString().c_str(), conn->ToString().c_str());

    AddConnection(conn);
    if (on_connection_cb_) {
      on_connection_cb_(conn);
    }

  } else {
    assert(0);
    // log ...
  }
}

int32_t TcpServer::Close() {
  AssertInLoopThread();
  vraft_logger.FInfo("tcp-server close, %s", DebugString().c_str());
  int32_t rv = 0;

  ConnectionMap tmp_conns = connections_;
  for (auto conn_pair : tmp_conns) {
    rv = conn_pair.second->Close();
    assert(rv == 0);
  }
  tmp_conns.clear();

  rv = acceptor_.Close();
  assert(rv == 0);

  return rv;
}

}  // namespace vraft
