#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>  // sockaddr_in

#include <csignal>
#include <iostream>

#include "raft.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"

//--------------------------------
// EXPECT_TRUE  true
// EXPECT_FALSE false
//
// EXPECT_EQ  ==
// EXPECT_NE  !=
// EXPECT_NE  <
// EXPECT_LE  <=
// EXPECT_GT  >
// EXPECT_GE  >=
//--------------------------------

TEST(HostPort, test) {
  vraft::HostPort hp("127.0.0.1:9988");
  std::cout << hp.host << std::endl;
  std::cout << hp.port << std::endl;
  std::cout << hp.ToString() << std::endl;
  EXPECT_EQ(hp.host, "127.0.0.1");
  EXPECT_EQ(hp.port, 9988);
  EXPECT_EQ(hp.ToString(), std::string("127.0.0.1:9988"));
}

TEST(HostPort, test2) {
  vraft::HostPort hp("127.0.0.1", 9988);
  std::cout << hp.host << std::endl;
  std::cout << hp.port << std::endl;
  std::cout << hp.ToString() << std::endl;
  EXPECT_EQ(hp.host, "127.0.0.1");
  EXPECT_EQ(hp.port, 9988);
  EXPECT_EQ(hp.ToString(), std::string("127.0.0.1:9988"));
}

TEST(HostPort, test3) {
  vraft::HostPort hp("localhost", 9988);
  std::cout << hp.host << std::endl;
  std::cout << hp.port << std::endl;
  std::cout << hp.ToString() << std::endl;
  EXPECT_EQ(hp.host, "localhost");
  EXPECT_EQ(hp.port, 9988);
  EXPECT_EQ(hp.ToString(), std::string("localhost:9988"));

  std::string ip32_str = vraft::IpU32ToIpString(hp.ip32);
  std::cout << ip32_str << std::endl;
  EXPECT_EQ(ip32_str, "127.0.0.1");

  EXPECT_EQ(((sockaddr_in *)(&hp.addr))->sin_family, AF_INET);
  EXPECT_EQ(ntohs(((sockaddr_in *)(&hp.addr))->sin_port), 9988);
  EXPECT_EQ(vraft::IpU32ToIpString(
                ntohl(((sockaddr_in *)(&hp.addr))->sin_addr.s_addr)),
            "127.0.0.1");
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}