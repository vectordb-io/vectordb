#include "connector.h"

#include <cassert>

namespace vraft {

void ConnectFinish(UvConnect *req, int32_t status) {
  Connector *c = reinterpret_cast<Connector *>(req->data);
  c->AssertInLoopThread();

  if (status == 0) {
    sockaddr_in local_addr, peer_addr;
    int namelen = sizeof(sockaddr_in);
    int rv = 0;

    rv = UvTcpGetSockName(c->conn_.get(), (struct sockaddr *)(&local_addr),
                          &namelen);
    assert(rv == 0);
    HostPort local = SockaddrInToHostPort(&local_addr);

    rv = UvTcpGetPeerName(c->conn_.get(), (struct sockaddr *)(&peer_addr),
                          &namelen);
    assert(rv == 0);
    HostPort peer = SockaddrInToHostPort(&peer_addr);

    vraft_logger.FInfo("connect ok, %s, local:%s, peer:%s",
                       c->DebugString().c_str(), local.ToString().c_str(),
                       peer.ToString().c_str());

    if (c->new_conn_func_) {
      c->new_conn_func_(std::move(c->conn_));
    }

    if (c->retry_timer_->Active()) {
      c->retry_timer_->Stop();
    }

  } else {
    vraft_logger.FError("connect error, %s", c->DebugString().c_str());
  }
}

void TimerConnectCb(Timer *timer) {
  Connector *c = reinterpret_cast<Connector *>(timer->data());
  c->AssertInLoopThread();

  if (timer->repeat_counter() > 0) {
    timer->RepeatDecr();
    int32_t r = c->Connect();
    if (r == 0) {
      // get result in ConnectFinish

    } else {
      vraft_logger.FError("connect error, %s", c->DebugString().c_str());
    }

  } else {
    vraft_logger.FError("connect error, %s, retry-timer stop",
                        c->DebugString().c_str());
    timer->Stop();
  }
}

void ConnectorCloseCb(UvHandle *handle) {
  assert(handle != nullptr);
  vraft_logger.FInfo("connector:%p close finish", handle);
}

Connector::Connector(EventLoopSPtr &loop, const HostPort &dest_addr,
                     const TcpOptions &options)
    : dest_addr_(dest_addr),
      options_(options),
      loop_(loop),
      new_conn_func_(nullptr),
      close_cb_(nullptr) {
  Init();
  vraft_logger.FInfo("connector construct, %s", DebugString().c_str());
}

Connector::~Connector() {
  vraft_logger.FInfo("connector destruct, %s", DebugString().c_str());
}

void Connector::AssertInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    return sptr->AssertInLoopThread();
  }
}

std::string Connector::DebugString() const {
  void *lptr = nullptr;
  auto sptr = loop_.lock();
  if (sptr) {
    lptr = sptr->UvLoopPtr();
  }
  char buf[256];
  int64_t retry = 0;
  if (retry_timer_) {
    retry = retry_timer_->repeat_counter();
  }
  snprintf(buf, sizeof(buf),
           "dest:%s, handle:%p, loop:%p, req:%p, retry_timer:%p, retry:%ld",
           dest_addr_.ToString().c_str(), conn_.get(), lptr, &connect_req_,
           retry_timer_.get(), retry);
  return std::string(buf);
}

int32_t Connector::TimerConnect(int64_t retry_times) {
  AssertInLoopThread();
  auto sptr = loop_.lock();
  if (sptr) {
    if (retry_times > 0) {
      retry_timer_->set_repeat_times(retry_times);
      sptr->AddTimer(retry_timer_);
    }
    return 0;

  } else {
    return -1;
  }
}

int32_t Connector::Connect(int64_t retry_times) {
  AssertInLoopThread();
  int32_t rv = 0;
  for (int64_t i = 0; i < retry_times; ++i) {
    rv = Connect();
    if (rv == 0) {
      break;
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(options_.retry_interval_ms));
  }
  return rv;
}

int32_t Connector::Connect() {
  AssertInLoopThread();
  return UvTcpConnect(&connect_req_, conn_.get(), &(dest_addr_.addr),
                      ConnectFinish);
}

int32_t Connector::Close() {
  AssertInLoopThread();
  vraft_logger.FInfo("connector close, %s", DebugString().c_str());

  retry_timer_->Close();

  if (conn_) {
    UvClose(reinterpret_cast<UvHandle *>(conn_.get()), ConnectorCloseCb);
  } else {
    vraft_logger.FInfo("connector close finish");
  }

  if (close_cb_) {
    close_cb_();
  }

  return 0;
}

void Connector::Init() {
  auto sptr = loop_.lock();
  if (sptr) {
    conn_ = std::make_unique<UvTcp>();
    UvTcpInit(sptr->UvLoopPtr(), conn_.get());
    connect_req_.data = this;

    TimerParam param;
    param.timeout_ms = 0;
    param.repeat_ms = options_.retry_interval_ms;
    param.cb = TimerConnectCb;
    param.data = this;
    retry_timer_ = sptr->MakeTimer(param);
    param.name = "connector-retry";
    assert(retry_timer_);
  }
}

}  // namespace vraft