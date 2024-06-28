#include "tracer.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

#include "coding.h"
#include "ping.h"
#include "ping_reply.h"
#include "raft.h"

TEST(Tracer, test) {
  system("rm -rf /tmp/raft_tracer_test");
  vraft::RaftConfig rc;
  rc.me = vraft::RaftAddr("127.0.0.1", 9000, 100);
  for (uint16_t i = 1; i <= 2; ++i) {
    uint16_t port = 9000 + i;
    rc.peers.push_back(vraft::RaftAddr("127.0.0.1", port, 100));
  }
  vraft::Raft r("/tmp/raft_tracer_test", rc);
  r.Init();

  vraft::RaftAddr src("127.0.0.1", 1234, 55);
  vraft::RaftAddr dest("127.0.0.1", 5678, 99);

  vraft::Ping msg;
  msg.src = src;
  msg.dest = dest;
  msg.msg = "ping";

  vraft::PingReply msg2;
  msg2.src = dest;
  msg2.dest = src;
  msg2.msg = "pang";

  std::string event_str = msg.ToJsonString(false, true);
  std::string event_str2 = msg2.ToJsonString(false, true);
  vraft::Tracer tracer(&r, true, nullptr);
  tracer.PrepareState0();
  tracer.PrepareEvent(vraft::kEventRecv, event_str);
  tracer.PrepareEvent(vraft::kEventTimer, "election-timer timeout");
  tracer.PrepareEvent(vraft::kEventTimer, "heartbeat-timer timeout");
  tracer.PrepareEvent(vraft::kEventSend, event_str2);
  tracer.PrepareState1();
  std::cout << tracer.Finish() << std::endl;
  system("rm -rf /tmp/raft_tracer_test");
}

TEST(Tracer, test2) {
  system("rm -rf /tmp/raft_tracer_test");
  vraft::RaftConfig rc;
  rc.me = vraft::RaftAddr("127.0.0.1", 9000, 100);
  for (uint16_t i = 1; i <= 2; ++i) {
    uint16_t port = 9000 + i;
    rc.peers.push_back(vraft::RaftAddr("127.0.0.1", port, 100));
  }
  vraft::Raft r("/tmp/raft_tracer_test", rc);
  r.Init();

  vraft::RaftAddr src("127.0.0.1", 1234, 55);
  vraft::RaftAddr dest("127.0.0.1", 5678, 99);

  vraft::Ping msg;
  msg.src = src;
  msg.dest = dest;
  msg.msg = "ping";

  vraft::PingReply msg2;
  msg2.src = dest;
  msg2.dest = src;
  msg2.msg = "pang";

  std::string event_str = msg.ToJsonString(false, true);
  std::string event_str2 = msg2.ToJsonString(false, true);
  vraft::Tracer tracer(&r, true, nullptr);
  tracer.EnableTimeStamp();
  tracer.PrepareState0();
  tracer.PrepareEvent(vraft::kEventRecv, event_str);
  tracer.PrepareEvent(vraft::kEventSend, event_str2);
  tracer.PrepareState1();
  std::cout << tracer.Finish() << std::endl;
  system("rm -rf /tmp/raft_tracer_test");
}

TEST(Tracer, test3) {
  system("rm -rf /tmp/raft_tracer_test");
  vraft::RaftConfig rc;
  rc.me = vraft::RaftAddr("127.0.0.1", 9000, 100);
  for (uint16_t i = 1; i <= 2; ++i) {
    uint16_t port = 9000 + i;
    rc.peers.push_back(vraft::RaftAddr("127.0.0.1", port, 100));
  }
  vraft::Raft r("/tmp/raft_tracer_test", rc);
  r.Init();

  vraft::RaftAddr src("127.0.0.1", 1234, 55);
  vraft::RaftAddr dest("127.0.0.1", 5678, 99);

  vraft::Ping msg;
  msg.src = src;
  msg.dest = dest;
  msg.msg = "ping";

  vraft::PingReply msg2;
  msg2.src = dest;
  msg2.dest = src;
  msg2.msg = "pang";

  std::string event_str = msg.ToJsonString(false, true);
  std::string event_str2 = msg2.ToJsonString(false, true);
  vraft::Tracer tracer(&r, false, nullptr);
  tracer.EnableTimeStamp();
  tracer.PrepareState0();
  tracer.PrepareEvent(vraft::kEventRecv, event_str);
  tracer.PrepareEvent(vraft::kEventSend, event_str2);
  tracer.PrepareState1();
  std::cout << "expect null [" << tracer.Finish() << "]" << std::endl;
  EXPECT_EQ(tracer.Finish(), std::string(""));
  system("rm -rf /tmp/raft_tracer_test");
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}