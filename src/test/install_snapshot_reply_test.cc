#include "install_snapshot_reply.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "util.h"

TEST(InstallSnapshotReply, InstallSnapshotReply) {
  uint32_t ip32;
  bool b = vraft::StringToIpU32("127.0.0.1", ip32);
  assert(b);

  vraft::RaftAddr src(ip32, 1234, 55);
  vraft::RaftAddr dest(ip32, 5678, 99);

  vraft::InstallSnapshotReply msg;
  msg.src = src;
  msg.dest = dest;
  msg.term = 100;
  msg.uid = vraft::UniqId(&msg);
  msg.send_ts = 100;
  msg.elapse = 200;
  msg.stored = 66;
  msg.req_term = 77;

  std::string msg_str;
  int32_t bytes = msg.ToString(msg_str);
  std::cout << "bytes:" << bytes << std::endl;

  std::cout << "encoding:" << std::endl;
  std::cout << msg.ToJsonString(true, true) << std::endl;
  std::cout << msg.ToJsonString(true, false) << std::endl;
  std::cout << msg.ToJsonString(false, true) << std::endl;
  std::cout << msg.ToJsonString(false, false) << std::endl;

  vraft::InstallSnapshotReply msg2;
  b = msg2.FromString(msg_str);
  assert(b);

  std::cout << "decoding:" << std::endl;
  std::cout << msg2.ToJsonString(true, true) << std::endl;
  std::cout << msg2.ToJsonString(true, false) << std::endl;
  std::cout << msg2.ToJsonString(false, true) << std::endl;
  std::cout << msg2.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(msg.src.ToU64(), msg2.src.ToU64());
  ASSERT_EQ(msg.dest.ToU64(), msg2.dest.ToU64());
  ASSERT_EQ(msg.term, msg2.term);
  ASSERT_EQ(msg.uid, msg2.uid);
  ASSERT_EQ(msg.stored, msg2.stored);
  ASSERT_EQ(msg.req_term, msg2.req_term);
  ASSERT_EQ(msg.send_ts, msg2.send_ts);
  ASSERT_EQ(msg.elapse, msg2.elapse);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}