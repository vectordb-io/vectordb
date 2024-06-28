#include "remu.h"

#include <cstdio>

#include "clock.h"
#include "raft_server.h"
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
  printf("--- global-state --- %s ---:\n", NsToString(Clock::NSec()).c_str());
  for (auto ptr : raft_servers) {
    ptr->Print(tiny, one_line);
    if (!one_line) {
      printf("\n");
    }
  }
  printf("\n");
  fflush(nullptr);
}

void Remu::Create() {
  for (auto conf : configs) {
    auto sptr = loop.lock();
    assert(sptr);
    vraft::RaftServerSPtr ptr = std::make_shared<vraft::RaftServer>(sptr, conf);
    ptr->raft()->set_tracer_cb(tracer_cb);
    ptr->raft()->set_create_sm(create_sm);
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

}  // namespace vraft
