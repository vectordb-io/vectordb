#include "acceptor.h"

#include <cassert>
#include <memory>

#include "eventloop.h"
#include "uv_wrapper.h"
#include "vraft_logger.h"

namespace vraft {

void AcceptorCloseCb(UvHandle *handle) {
  assert(handle != nullptr);
  Acceptor *acceptor = reinterpret_cast<Acceptor *>(handle->data);
  if (acceptor->close_cb_) {
    acceptor->close_cb_();
  }

  vraft_logger.FInfo("acceptor:%p close finish", handle);
}

Acceptor::Acceptor(EventLoopSPtr &loop, const HostPort &addr,
                   const TcpOptions &options)
    : addr_(addr), options_(options), loop_(loop) {
  vraft_logger.FInfo("acceptor construct, %s", DebugString().c_str());
  Init();
}

Acceptor::~Acceptor() {
  vraft_logger.FInfo("acceptor destruct, %s", DebugString().c_str());
}

void Acceptor::AssertInLoopThread() const {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
  }
}

std::string Acceptor::DebugString() const {
  void *lptr = nullptr;
  auto sptr = loop_.lock();
  if (sptr) {
    lptr = sptr->UvLoopPtr();
  }
  char buf[256];
  snprintf(buf, sizeof(buf), "addr:%s, handle:%p, loop:%p",
           addr_.ToString().c_str(), &server_, lptr);
  return std::string(buf);
}

int32_t Acceptor::Start() {
  AssertInLoopThread();
  vraft_logger.FInfo("acceptor start, %s", DebugString().c_str());

  int32_t rv = Bind();
  assert(rv == 0);

  rv = Listen();
  assert(rv == 0);

  return rv;
}

int32_t Acceptor::Close() {
  AssertInLoopThread();
  vraft_logger.FInfo("acceptor close, %s", DebugString().c_str());

  if (!UvIsClosing(reinterpret_cast<UvHandle *>(&server_))) {
    UvClose(reinterpret_cast<UvHandle *>(&server_), AcceptorCloseCb);
  } else {
    vraft_logger.FInfo("acceptor close, already closing, %s",
                       DebugString().c_str());
  }

  return 0;
}

bool Acceptor::Active() {
  return UvIsActive(reinterpret_cast<UvHandle *>(&server_));
}

// Acceptor::NewConnection is a function broker
// called by AcceptorHandleRead
// to call TcpServer::NewConnection
void Acceptor::NewConnection(UvTcpUPtr conn) {
  AssertInLoopThread();

  assert(new_conn_func_);
  new_conn_func_(std::move(conn));
}

UvLoop *Acceptor::UvLoopPtr() {
  auto sptr = loop_.lock();
  if (sptr) {
    return sptr->UvLoopPtr();
  } else {
    return nullptr;
  }
}

void Acceptor::Init() {
  auto sptr = loop_.lock();
  if (sptr) {
    UvTcpInit(sptr->UvLoopPtr(), &server_);
    if (options_.tcp_nodelay) {
      UvTcpNodelay(&server_, 1);
    }
    server_.data = this;
  }
}

int32_t Acceptor::Bind() {
  AssertInLoopThread();
  int32_t rv = UvTcpBind(&server_, &(addr_.addr), 0);
  return rv;
}

int32_t Acceptor::Listen() {
  AssertInLoopThread();
  int32_t rv = UvListen((UvStream *)&server_, 128, AcceptorHandleRead);
  return rv;
}

void AcceptorHandleRead(UvStream *server, int status) {
  Acceptor *acceptor = static_cast<Acceptor *>(server->data);
  acceptor->AssertInLoopThread();

  if (status == 0) {
    UvLoop *lptr = acceptor->UvLoopPtr();
    if (lptr != nullptr) {
      UvTcpUPtr client = std::make_unique<UvTcp>();
      int32_t rv = UvTcpInit(lptr, client.get());
      assert(rv == 0);

      if (acceptor->options().tcp_nodelay) {
        UvTcpNodelay(client.get(), 1);
      }

      if (UvAccept(server, (UvStream *)client.get()) == 0) {
        acceptor->NewConnection(std::move(client));

      } else {
        // UvTcpUPtr will free UvTcp, so need not OnClose
        UvClose(reinterpret_cast<UvHandle *>(client.get()), nullptr);
      }
    }

  } else {
    assert(0);
  }
}

}  // namespace vraft
