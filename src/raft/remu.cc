#include "remu.h"

#include <cstdio>
#include <unordered_map>

#include "clock.h"
#include "raft_server.h"
#include "test_suite.h"
#include "util.h"

namespace vraft {

void Remu::Log(std::string key) {
  uint64_t ts = Clock::NSec();
  char ts_buf[64];
  if (key.empty()) {
    ts = Clock::NSec();
    snprintf(ts_buf, sizeof(ts_buf), "0x%lX", ts);
  } else {
    snprintf(ts_buf, sizeof(ts_buf), "%s", key.c_str());
  }

  std::string str;
  str.append("\n");
  str.append(ts_buf);
  str.append(" global-state: ");
  str.append(NsToString(ts));

  for (auto ptr : raft_servers) {
    str.append("\n");
    str.append(ts_buf);
    str.append(" global-state: ");
    str.append(ptr->raft()->ToJsonString(true, true));
  }
  vraft_logger.FInfo("%s", str.c_str());
}

void Remu::Print(bool tiny, bool one_line) {
  printf("--- remu global-state --- %s ---:\n",
         NsToString(Clock::NSec()).c_str());
  for (auto ptr : raft_servers) {
    ptr->Print(tiny, one_line);
    if (!one_line) {
      printf("\n");
    }
  }
  printf("\n");
  fflush(nullptr);
}

void Remu::PrintConfig() {
  for (auto conf : configs) {
    std::cout << conf.ToString() << std::endl;
  }
}

void Remu::Check() {
  CheckLeader();
  CheckLog();
  CheckMeta();
  CheckIndex();
}

void Remu::CheckLeader() {
  std::unordered_map<RaftTerm, RaftAddr> leader_map;
  for (auto &raft_server : raft_servers) {
    if (raft_server->raft()->state() == STATE_LEADER) {
      RaftTerm term = raft_server->raft()->Term();
      RaftAddr addr = raft_server->raft()->Me();
      auto it = leader_map.find(term);
      if (it == leader_map.end()) {
        leader_map[term] = addr;
      } else {
        std::cout << "check error, term:" << term
                  << ", leader:" << it->second.ToString() << " "
                  << addr.ToString() << std::endl
                  << std::flush;
        assert(0);
      }
    }
  }
}

void Remu::CheckLog() {}

void Remu::CheckMeta() {}

void Remu::CheckIndex() {}

int32_t Remu::LeaderTimes() {
  int32_t times = 0;
  for (auto &ptr : raft_servers) {
    if (ptr) {
      times += ptr->raft()->leader_times();
    }
  }
  return times;
}

void Remu::Create() {
  for (auto conf : configs) {
    auto sptr = loop.lock();
    assert(sptr);
    vraft::RaftServerSPtr ptr = std::make_shared<vraft::RaftServer>(sptr, conf);
    ptr->raft()->set_tracer_cb(tracer_cb);
    ptr->raft()->set_create_sm(create_sm);
    ptr->raft()->set_enable_pre_vote(enable_pre_vote_);
    ptr->raft()->set_interval_check(interval_check_);
    if (conf.peers().size() == 0) {
      ptr->raft()->set_standby(true);
    }
    raft_servers.push_back(ptr);
  }
}

void Remu::Start() {
  for (auto &ptr : raft_servers) {
    if (ptr) {
      ptr->Start();
    }
  }
}

void Remu::Stop() {
  for (auto &ptr : raft_servers) {
    if (ptr) {
      ptr->Stop();
    }
  }
}

void Remu::Clear() {
  configs.clear();
  raft_servers.clear();
}

void Remu::AddOneNode() {
  assert(configs.size() > 0);

  vraft::Config c;
  c.set_my_addr(HostPort(configs[0].my_addr().host, standby_port++));
  c.set_log_level(kLoggerTrace);
  c.set_enable_debug(true);
  c.set_path(vraft::gtest_path + "/" + c.my_addr().ToString());
  c.set_mode(kSingleMode);

  configs.push_back(c);
}

#if 0
void Remu::AddOneNode() {
  assert(configs.size() > 0);
  vraft::Config c;
  uint16_t max_port = configs[0].my_addr().port;
  for (auto peer : configs[0].peers()) {
    if (peer.port > max_port) {
      max_port = peer.port;
    }
  }

  c.set_my_addr(HostPort(configs[0].my_addr().host, max_port + 1));
  c.set_log_level(kLoggerTrace);
  c.set_enable_debug(true);
  c.set_path(vraft::gtest_path);
  c.set_mode(kSingleMode);

  configs.push_back(c);
}
#endif

}  // namespace vraft
