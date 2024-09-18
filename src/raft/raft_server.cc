#include "raft_server.h"

#include "ping.h"
#include "ping_reply.h"
#include "vraft_logger.h"

namespace vraft {

RaftServer::RaftServer(EventLoopSPtr &loop, Config &config)
    : config_(config), loop_(loop), enable_send_(true), enable_recv_(true) {
  Init();
  vraft_logger.FInfo("raft-server construct, loop:%p, %s, %p",
                     loop->UvLoopPtr(), config.my_addr().ToString().c_str(),
                     this);
}

RaftServer::~RaftServer() {
  vraft_logger.FInfo("raft-server destruct, %p", this);
}

void RaftServer::OnConnection(const vraft::TcpConnectionSPtr &conn) {
  vraft::vraft_logger.FInfo("raft-server on connection:%s",
                            conn->name().c_str());
}

void RaftServer::OnMessage(const vraft::TcpConnectionSPtr &conn,
                           vraft::Buffer *buf) {
  uint64_t recv_ns = Clock::NSec();
  uint64_t diff_ms = 0;
  vraft_logger.FTrace("raft-server recv msg, readable-bytes:%d",
                      buf->ReadableBytes());

  {
    int32_t print_bytes = buf->ReadableBytes();
    std::string end_str = "";
    if (print_bytes > MAX_DEBUG_LEN) {
      print_bytes = MAX_DEBUG_LEN;
      end_str = " ...";
    }
    vraft_logger.FDebug("recv buf data:%s%s",
                        StrToHexStr(buf->BeginRead(), print_bytes).c_str(),
                        end_str.c_str());
  }

  while (buf->ReadableBytes() >= static_cast<int32_t>(sizeof(MsgHeader))) {
    int32_t body_bytes = buf->PeekInt32();
    vraft_logger.FTrace(
        "raft-server recv msg, readable-bytes:%d, body_bytes:%d",
        buf->ReadableBytes(), body_bytes);

    if (buf->ReadableBytes() >=
        static_cast<int32_t>(sizeof(MsgHeader)) + body_bytes) {
      // parse header
      MsgHeader header;
      header.FromString(buf->BeginRead(), sizeof(MsgHeader));
      buf->Retrieve(sizeof(MsgHeader));

      // parse body
      switch (header.type) {
        case kPing: {
          Ping msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnPing(msg);
          }
          break;
        }

        case kPingReply: {
          PingReply msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnPingReply(msg);
          }
          break;
        }

        case kRequestVote: {
          RequestVote msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnRequestVote(msg);
          }
          break;
        }

        case kRequestVoteReply: {
          RequestVoteReply msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnRequestVoteReply(msg);
          }
          break;
        }

        case kAppendEntries: {
          AppendEntries msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnAppendEntries(msg);
          }
          break;
        }

        case kAppendEntriesReply: {
          AppendEntriesReply msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnAppendEntriesReply(msg);
          }
          break;
        }

        case kInstallSnapshot: {
          InstallSnapshot msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnInstallSnapshot(msg);
          }
          break;
        }

        case kInstallSnapshotReply: {
          InstallSnapshotReply msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnInstallSnapshotReply(msg);
          }
          break;
        }

        case kTimeoutNow: {
          TimeoutNow msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          diff_ms = (recv_ns - msg.send_ts) / (1000 * 1000);
          msg.elapse = diff_ms;
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnTimeoutNow(msg);
          }
          break;
        }

        case kClientRequet: {
          ClientRequest msg;
          int32_t bytes = msg.FromString(buf->BeginRead(), body_bytes);
          assert(bytes > 0);
          buf->Retrieve(body_bytes);
          if (enable_recv_) {
            raft_->OnClientRequest(msg, conn);
          }
          break;
        }

        default:
          assert(0);
      }
    }
  }

  vraft_logger.FTrace("raft-server after process msg, readable-bytes:%d",
                      buf->ReadableBytes());
}

void RaftServer::Init() {
  struct TcpOptions options;
  auto sptr = loop_.lock();
  assert(sptr);
  server_ = std::make_shared<TcpServer>(sptr, "raft-server", config_.my_addr(),
                                        options);

  server_->set_on_connection_cb(
      std::bind(&RaftServer::OnConnection, this, std::placeholders::_1));
  server_->set_on_message_cb(std::bind(&RaftServer::OnMessage, this,
                                       std::placeholders::_1,
                                       std::placeholders::_2));

  RaftConfig rc;
  RaftAddr me(config_.my_addr().ip32, config_.my_addr().port, 0);
  rc.me = me;
  for (auto hostport : config_.peers()) {
    RaftAddr dest(hostport.ip32, hostport.port, 0);
    rc.peers.push_back(dest);
  }

  raft_ = std::make_shared<Raft>(config_.path(), rc);
  raft_->Init();
  raft_->set_send(std::bind(&RaftServer::Send, this, std::placeholders::_1,
                            std::placeholders::_2, std::placeholders::_3));
  raft_->set_make_timer(
      std::bind(&RaftServer::MakeTimer, this, std::placeholders::_1));

  raft_->set_enable_send_func(std::bind(&RaftServer::EnableSend, this));
  raft_->set_disable_send_func(std::bind(&RaftServer::DisableSend, this));
  raft_->set_enable_recv_func(std::bind(&RaftServer::EnableRecv, this));
  raft_->set_disable_recv_func(std::bind(&RaftServer::DisableRecv, this));
  raft_->EnableSend();
  raft_->EnableRecv();

  raft_->set_assert_loop(std::bind(&RaftServer::AssertInLoopThread, this));
}

int32_t RaftServer::Start() {
  int32_t rv = 0;
  rv = server_->Start();
  assert(rv == 0);

  rv = raft_->Start();
  assert(rv == 0);

  return rv;
}

int32_t RaftServer::Stop() {
  int32_t rv = raft_->Stop();
  assert(rv == 0);

  for (auto &c : clients_) {
    c.second->Stop();
  }
  server_->Stop();

  return rv;
}

// return 0: ok
// return -1: error
// return -2: enable send
int32_t RaftServer::Send(uint64_t dest_addr, const char *buf,
                         unsigned int size) {
  if (enable_send_) {
    int32_t rv = 0;
    TcpClientSPtr client = GetClientOrCreate(dest_addr);
    if (client) {
      rv = client->CopySend(buf, size);
    } else {
      rv = -1;
      vraft_logger.FError("GetClientOrCreate error");
    }
    return rv;

  } else {
    return -2;
  }
}

TimerSPtr RaftServer::MakeTimer(TimerParam &param) {
  auto sptr = loop_.lock();
  if (sptr) {
    return sptr->MakeTimer(param);
  } else {
    return nullptr;
  }
}

TcpClientSPtr RaftServer::GetClient(uint64_t dest_addr) {
  auto it = clients_.find(dest_addr);
  if (it != clients_.end()) {
    return it->second;

  } else {
    // not found
    return nullptr;
  }
}

TcpClientSPtr RaftServer::GetClientOrCreate(uint64_t dest_addr) {
  TcpClientSPtr ptr = GetClient(dest_addr);
  if (ptr && !ptr->Connected()) {
    /*
        printf("RaftServer::GetClientOrCreate %s clients_.erase %s -------- \n",
               raft_->Me().ToString().c_str(),
               RaftAddr(dest_addr).ToString().c_str());
    */
    clients_.erase(dest_addr);
    ptr.reset();
  }

  if (!ptr) {
    struct TcpOptions options;
    RaftAddr addr(dest_addr);
    HostPort hostport(IpU32ToIpString(addr.ip()), addr.port());
    auto sptr = loop_.lock();
    assert(sptr);
    ptr = std::make_shared<TcpClient>(sptr, "raft-client", hostport, options);
    int32_t rv = ptr->Connect(100);
    if (rv == 0) {
      clients_.insert({dest_addr, ptr});

      /*
            printf("RaftServer::GetClientOrCreate %s clients_.insert %s --------
         \n", raft_->Me().ToString().c_str(),
                   RaftAddr(dest_addr).ToString().c_str());
      */

    } else {
      printf("RaftServer::GetClientOrCreate %s ptr.reset %s -------- \n",
             raft_->Me().ToString().c_str(),
             RaftAddr(dest_addr).ToString().c_str());

      ptr.reset();
    }
  }

  return ptr;
}

void RaftServer::AssertInLoopThread() {
  auto sptr = loop_.lock();
  if (sptr) {
    sptr->AssertInLoopThread();
  }
}

void RaftServer::DisableSend() { set_enable_send(false); }

void RaftServer::EnableSend() { set_enable_send(true); }

void RaftServer::DisableRecv() { set_enable_recv(false); }

void RaftServer::EnableRecv() { set_enable_recv(true); }

bool RaftServer::enable_send() const { return enable_send_; }

void RaftServer::set_enable_send(bool enable_send) {
  enable_send_ = enable_send;
}

bool RaftServer::enable_recv() const { return enable_recv_; }

void RaftServer::set_enable_recv(bool enable_recv) {
  enable_recv_ = enable_recv;
}

}  // namespace vraft
