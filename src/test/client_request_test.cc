#include "client_request.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "util.h"

TEST(ClientRequest, ClientRequest) {
  vraft::ClientRequest msg;
  msg.uid = vraft::UniqId(&msg);
  msg.cmd = vraft::kCmdLeaderTransfer;
  msg.data = "127.0.0.1:9988#7";

  std::string msg_str;
  int32_t bytes = msg.ToString(msg_str);
  std::cout << "bytes:" << bytes << std::endl;

  std::cout << "encoding:" << std::endl;
  std::cout << msg.ToJsonString(true, true) << std::endl;
  std::cout << msg.ToJsonString(true, false) << std::endl;
  std::cout << msg.ToJsonString(false, true) << std::endl;
  std::cout << msg.ToJsonString(false, false) << std::endl;

  vraft::ClientRequest msg2;
  int32_t rv = msg2.FromString(msg_str);
  assert(rv > 0);

  std::cout << "decoding:" << std::endl;
  std::cout << msg2.ToJsonString(true, true) << std::endl;
  std::cout << msg2.ToJsonString(true, false) << std::endl;
  std::cout << msg2.ToJsonString(false, true) << std::endl;
  std::cout << msg2.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(msg.uid, msg2.uid);
  ASSERT_EQ(msg.cmd, msg2.cmd);
  ASSERT_EQ(msg.data, msg2.data);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}