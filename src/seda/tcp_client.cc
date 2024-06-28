#include "tcp_client.h"

namespace vraft {

TcpClient::TcpClient(EventLoopSPtr &loop, const std::string name,
                     const HostPort &dest_addr, const TcpOptions &options)
    : name_(name),
      dest_addr_(dest_addr),
      loop_(loop),
      connector_(loop, dest_addr, options) {
  vraft_logger.FInfo("tcp-client construct, %s", DebugString().c_str());
  Init();
}

TcpClient::~TcpClient() {
  vraft_logger.FInfo("tcp-client destruct, %s", DebugString().c_str());
}

void TcpClient::Stop() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->RunFunctor(std::bind(&TcpClient::Close, this));
  }
}

void TcpClient::RunFunctor(const Functor func) {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->RunFunctor(func);
  }
}

void TcpClient::AssertInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
  }
}

std::string TcpClient::DebugString() const {
  void *lptr = nullptr;
  auto sptr = loop_.lock();
  if (sptr) {
    lptr = sptr->UvLoopPtr();
  }

  std::string conn = "null";
  if (connection_) {
    conn = connection_->ToString();
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "name:%s, loop:%p, connector:%s, conn:%s",
           name_.c_str(), lptr, connector_.DebugString().c_str(), conn.c_str());
  return std::string(buf);
}

std::string TcpClient::ToString() const {
  if (connection_) {
    return connection_->ToString();
  } else {
    return "local:-peer:";
  }
}

int32_t TcpClient::TimerConnect(int64_t retry_times) {
  AssertInLoopThread();
  connector_.set_close_cb(close_cb_);
  return connector_.TimerConnect(retry_times);
}

int32_t TcpClient::Connect(int64_t retry_times) {
  AssertInLoopThread();
  connector_.set_close_cb(close_cb_);
  return connector_.Connect(retry_times);
}

int32_t TcpClient::Connect() {
  AssertInLoopThread();
  connector_.set_close_cb(close_cb_);
  return connector_.Connect();
}

bool TcpClient::Connected() const {
  if (connection_) {
    return connection_->Connected();
  } else {
    return false;
  }
}

void TcpClient::RemoveConnection(const TcpConnectionSPtr &conn) {
  AssertInLoopThread();
  connection_.reset();
}

int32_t TcpClient::Send(const char *buf, unsigned int size) {
  AssertInLoopThread();
  if (connection_) {
    return connection_->Send(buf, size);
  } else {
    return -1;
  }
}

int32_t TcpClient::CopySend(const char *buf, unsigned int size) {
  AssertInLoopThread();
  if (connection_) {
    return connection_->CopySend(buf, size);
  } else {
    return -1;
  }
}

int32_t TcpClient::BufSend(const char *buf, unsigned int size) {
  AssertInLoopThread();
  if (connection_) {
    return connection_->BufSend(buf, size);
  } else {
    return -1;
  }
}

void TcpClient::Init() {
  connector_.set_new_conn_func(
      std::bind(&TcpClient::NewConnection, this, std::placeholders::_1));
}

void TcpClient::NewConnection(UvTcpUPtr client) {
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
  assert(sptr);
  connection_ =
      std::make_shared<TcpConnection>(sptr, conn_name, std::move(client));
  connection_->set_on_connection_cb(on_connection_cb_);
  connection_->set_on_message_cb(on_message_cb_);
  connection_->set_write_complete_cb(write_complete_cb_);
  connection_->set_connection_close_cb(
      std::bind(&TcpClient::RemoveConnection, this, std::placeholders::_1));
  vraft_logger.FInfo("tcp-client:%s new connection:%s", name_.c_str(),
                     connection_->ToString().c_str());

  int32_t r = connection_->Start();
  assert(r == 0);

  if (on_connection_cb_) {
    on_connection_cb_(connection_);
  }
}

int32_t TcpClient::Close() {
  AssertInLoopThread();
  vraft_logger.FInfo("tcp-client close, %s", DebugString().c_str());
  int32_t rv = 0;

  if (connection_) {
    rv = connection_->Close();
    assert(rv == 0);
  }

  rv = connector_.Close();
  assert(rv == 0);

  return rv;
}

}  // namespace vraft