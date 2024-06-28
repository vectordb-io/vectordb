#include "tcp_connection.h"

#include <cassert>
#include <iostream>

#include "allocator.h"
#include "eventloop.h"
#include "hostport.h"
#include "vraft_logger.h"

namespace vraft {

void TcpConnectionHandleRead(UvStream *client, ssize_t nread,
                             const UvBuf *buf) {
  TcpConnection *conn = static_cast<TcpConnection *>(client->data);
  conn->AssertInLoopThread();

  if (nread > 0) {
    uint32_t u32 = Crc32(buf->base, nread);
    vraft_logger.FTrace("recv data, handle:%p, nread:%d, check:%X", client,
                        nread, u32);
    vraft_logger.FDebug("recv data:%s", StrToHexStr(buf->base, nread).c_str());
    conn->OnMessage(buf->base, nread);

  } else {
    vraft_logger.FError("read error, handle:%p, nread:%d, err:%s", client,
                        nread, UvStrError(nread));
    UvClose(reinterpret_cast<UvHandle *>(client), HandleClientClose);
  }
}

void TcpConnectionAllocBuffer(UvHandle *client, size_t suggested_size,
                              UvBuf *buf) {
  TcpConnection *conn = static_cast<TcpConnection *>(client->data);
  conn->AssertInLoopThread();

  buf->base = (char *)conn->allocator().Malloc(suggested_size);
  buf->len = suggested_size;
}

void TcpConnectionAllocBuffer2(UvHandle *client, size_t suggested_size,
                               UvBuf *buf) {
  TcpConnection *conn = static_cast<TcpConnection *>(client->data);
  conn->AssertInLoopThread();

  buf->base = const_cast<char *>(conn->input_buf_.BeginWrite());
  buf->len = conn->input_buf_.WritableBytes();
}

void WriteComplete(UvWrite *req, int status) {
  TcpConnection *conn = static_cast<TcpConnection *>(req->handle->data);
  conn->AssertInLoopThread();

  assert(status == 0);
  WriteReq *wr = reinterpret_cast<WriteReq *>(req);

  // only free wr
  // user need to free buf
  conn->allocator().Free(wr);

  if (conn->write_complete_cb_) {
    conn->write_complete_cb_(conn->shared_from_this());
  }
}

void BufWriteComplete(UvWrite *req, int status) {
  TcpConnection *conn = static_cast<TcpConnection *>(req->handle->data);
  conn->AssertInLoopThread();

  assert(status == 0);
  WriteReq *wr = reinterpret_cast<WriteReq *>(req);
  conn->allocator().Free(wr->buf.base);
  conn->allocator().Free(wr);

  if (conn->write_complete_cb_) {
    conn->write_complete_cb_(conn->shared_from_this());
  }
}

void HandleClientClose(UvHandle *handle) {
  TcpConnection *conn = static_cast<TcpConnection *>(handle->data);
  vraft_logger.FInfo("connection:%p close finish", handle);
  conn->AssertInLoopThread();
  if (conn->connection_close_cb_) {
    TcpConnectionSPtr cptr = conn->shared_from_this();
    conn->connection_close_cb_(cptr);
  }
}

TcpConnection::TcpConnection(EventLoopSPtr &loop, const std::string &name,
                             UvTcpUPtr conn)
    : name_(name), loop_(loop), allocator_(), conn_(std::move(conn)) {
  conn_->data = this;
  sockaddr_in local_addr, peer_addr;
  int namelen = sizeof(sockaddr_in);
  int rv = 0;

  rv =
      UvTcpGetSockName(conn_.get(), (struct sockaddr *)(&local_addr), &namelen);
  assert(rv == 0);
  local_addr_ = SockaddrInToHostPort(&local_addr);

  rv = UvTcpGetPeerName(conn_.get(), (struct sockaddr *)(&peer_addr), &namelen);
  assert(rv == 0);
  peer_addr_ = SockaddrInToHostPort(&peer_addr);

  vraft_logger.FInfo("connection construct, %s", DebugString().c_str());
}

TcpConnection::~TcpConnection() {
  vraft_logger.FInfo("connection destruct, %s", DebugString().c_str());
}

int32_t TcpConnection::Start() {
  AssertInLoopThread();
  return UvReadStart(reinterpret_cast<UvStream *>(conn_.get()),
                     TcpConnectionAllocBuffer2, TcpConnectionHandleRead);
}

int32_t TcpConnection::Close() {
  AssertInLoopThread();
  if (!UvIsClosing(reinterpret_cast<UvHandle *>(conn_.get()))) {
    UvClose(reinterpret_cast<UvHandle *>(conn_.get()), HandleClientClose);
  } else {
    vraft_logger.FInfo("connection close, already closing, %s",
                       DebugString().c_str());
  }
  return 0;
}

bool TcpConnection::Connected() const {
  return UvIsActive(reinterpret_cast<UvHandle *>(conn_.get())) &&
         UvIsReadable(reinterpret_cast<UvStream *>(conn_.get())) &&
         UvIsWritable(reinterpret_cast<UvStream *>(conn_.get()));
}

int32_t TcpConnection::Send(const char *buf, ssize_t size) {
  AssertInLoopThread();

  if (!Connected()) {
    vraft_logger.FError("connection send error, not connected, %s",
                        DebugString().c_str());
    return -1;
  }

  WriteReq *write_req = (WriteReq *)allocator_.Malloc(sizeof(WriteReq));
  write_req->buf =
      UvBufInit(const_cast<char *>(buf), static_cast<unsigned int>(size));
  UvWrite2(reinterpret_cast<UvWrite *>(write_req),
           reinterpret_cast<UvStream *>(conn_.get()), &write_req->buf, 1,
           WriteComplete);
  return 0;
}

int32_t TcpConnection::CopySend(const char *buf, ssize_t size) {
  AssertInLoopThread();

  if (!Connected()) {
    vraft_logger.FError("connection copy-send error, not connected, %s",
                        DebugString().c_str());
    return -1;
  }

  char *send_buf = (char *)allocator_.Malloc(size);
  memcpy(send_buf, buf, size);

  WriteReq *write_req = (WriteReq *)allocator_.Malloc(sizeof(WriteReq));
  write_req->buf =
      UvBufInit(const_cast<char *>(send_buf), static_cast<unsigned int>(size));
  UvWrite2(reinterpret_cast<UvWrite *>(write_req),
           reinterpret_cast<UvStream *>(conn_.get()), &write_req->buf, 1,
           BufWriteComplete);
  return 0;
}

int32_t TcpConnection::BufSend(const char *buf, ssize_t size) {
  AssertInLoopThread();

  if (!Connected()) {
    vraft_logger.FError("connection buf-send error, not connected, %s",
                        DebugString().c_str());
    return -1;
  }

  char *send_buf = (char *)allocator_.Malloc(size);
  memcpy(send_buf, buf, size);

  WriteReq *write_req = (WriteReq *)allocator_.Malloc(sizeof(WriteReq));
  write_req->buf =
      UvBufInit(const_cast<char *>(send_buf), static_cast<unsigned int>(size));
  UvWrite2(reinterpret_cast<UvWrite *>(write_req),
           reinterpret_cast<UvStream *>(conn_.get()), &write_req->buf, 1,
           BufWriteComplete);
  return 0;
}

void TcpConnection::OnMessage(const char *buf, ssize_t size) {
  AssertInLoopThread();

  input_buf_.Append(buf, size);
  if (on_message_cb_) {
    on_message_cb_(shared_from_this(), &input_buf_);
  } else {
    input_buf_.RetrieveAll();
  }
}

void TcpConnection::OnMessage(Buffer &buf) {
  AssertInLoopThread();

  if (on_message_cb_) {
    on_message_cb_(shared_from_this(), &input_buf_);
  }
}

void TcpConnection::AssertInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
  }
}

bool TcpConnection::IsInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    return sptr->IsInLoopThread();
  } else {
    return false;
  }
}

std::string TcpConnection::DebugString() {
  void *lptr = nullptr;
  auto sptr = loop_.lock();
  if (sptr) {
    lptr = sptr->UvLoopPtr();
  }
  char buf[256];
  snprintf(buf, sizeof(buf),
           "name:%s, local:%s, peer:%s, loop:%p, handle:%p, readable:%d, "
           "writable:%d, waste:%d",
           name_.c_str(), local_addr_.ToString().c_str(),
           peer_addr_.ToString().c_str(), lptr, conn_.get(),
           input_buf_.ReadableBytes(), input_buf_.WritableBytes(),
           input_buf_.WastefulBytes());
  return std::string(buf);
}

UvLoop *TcpConnection::UvLoopPtr() {
  auto sptr = loop_.lock();
  if (sptr) {
    return sptr->UvLoopPtr();
  } else {
    return nullptr;
  }
}

EventLoopSPtr TcpConnection::LoopSPtr() {
  auto sptr = loop_.lock();
  return sptr;
}

}  // namespace vraft
