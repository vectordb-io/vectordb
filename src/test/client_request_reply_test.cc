#include "client_request_reply.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "util.h"

TEST(ClientRequestReply, ClientRequestReply) {
  vraft::ClientRequestReply msg;
  msg.uid = vraft::UniqId(&msg);
  msg.code = 3;
  msg.msg = "not leader";
  msg.data = "127.0.0.1:9988#7";

  std::string msg_str;
  int32_t bytes = msg.ToString(msg_str);
  std::cout << "bytes:" << bytes << std::endl;

  std::cout << "encoding:" << std::endl;
  std::cout << msg.ToJsonString(true, true) << std::endl;
  std::cout << msg.ToJsonString(true, false) << std::endl;
  std::cout << msg.ToJsonString(false, true) << std::endl;
  std::cout << msg.ToJsonString(false, false) << std::endl;

  vraft::ClientRequestReply msg2;
  int32_t rv = msg2.FromString(msg_str);
  assert(rv > 0);

  std::cout << "decoding:" << std::endl;
  std::cout << msg2.ToJsonString(true, true) << std::endl;
  std::cout << msg2.ToJsonString(true, false) << std::endl;
  std::cout << msg2.ToJsonString(false, true) << std::endl;
  std::cout << msg2.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(msg.uid, msg2.uid);
  ASSERT_EQ(msg.code, msg2.code);
  ASSERT_EQ(msg.msg, msg2.msg);
  ASSERT_EQ(msg.data, msg2.data);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}