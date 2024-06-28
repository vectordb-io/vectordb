#include "parser.h"

#include <gtest/gtest.h>

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
// ASSERT_TRUE  true
// ASSERT_FALSE false
//
// EXPECT_EQ  ==
// EXPECT_NE  !=
// EXPECT_NE  <
// EXPECT_LE  <=
// EXPECT_GT  >
// EXPECT_GE  >=
//
// ASSERT_EQ  ==
// ASSERT_NE  !=
// ASSERT_LT  <
// ASSERT_LE  <=
// ASSERT_GT  >
// ASSERT_GE  >=
//--------------------------------

TEST(Parser, kCmdError) {
  vectordb::Parser parser("xx");
  ASSERT_EQ(parser.cmd(), vectordb::kCmdError);
  std::cout << parser.ToJsonString(false, false) << std::endl;
}

TEST(Parser, kCmdHelp) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdHelp));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdHelp);
  std::cout << parser.ToJsonString(false, false) << std::endl;
}

TEST(Parser, kCmdVersion) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdVersion));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdVersion);
  std::cout << parser.ToJsonString(false, false) << std::endl;
}

TEST(Parser, kCmdQuit) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdQuit));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdQuit);
  std::cout << parser.ToJsonString(false, false) << std::endl;
}

TEST(Parser, kCmdMeta) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdMeta));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdMeta);
  std::cout << parser.ToJsonString(false, false) << std::endl;
}

TEST(Parser, kCmdCreateTable) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdCreateTable));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdCreateTable);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.name(), "test-table");
  ASSERT_EQ(parser.partition_num(), 10);
  ASSERT_EQ(parser.replica_num(), 3);
}

TEST(Parser, kCmdBuildIndex) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdBuildIndex));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdBuildIndex);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.annoy_tree_num(), 10);
}

TEST(Parser, kShowTables) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kShowTables));
  ASSERT_EQ(parser.cmd(), vectordb::kShowTables);
  std::cout << parser.ToJsonString(false, false) << std::endl;
}

TEST(Parser, kShowPartitions) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kShowPartitions));
  ASSERT_EQ(parser.cmd(), vectordb::kShowPartitions);
  std::cout << parser.ToJsonString(false, false) << std::endl;
}

TEST(Parser, kShowReplicas) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kShowReplicas));
  ASSERT_EQ(parser.cmd(), vectordb::kShowReplicas);
  std::cout << parser.ToJsonString(false, false) << std::endl;
}

TEST(Parser, kDescTable) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kDescTable));
  ASSERT_EQ(parser.cmd(), vectordb::kDescTable);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.name(), "test-table");
}

TEST(Parser, kDescPartition) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kDescPartition));
  ASSERT_EQ(parser.cmd(), vectordb::kDescPartition);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.name(), "test-table#0");
}

TEST(Parser, kDescDescReplica) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kDescDescReplica));
  ASSERT_EQ(parser.cmd(), vectordb::kDescDescReplica);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.name(), "test-table#0#0");
}

TEST(Parser, kCmdPut) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdPut));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdPut);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.key(), "key_0");
  ASSERT_EQ(parser.table(), "test-table");
  ASSERT_EQ(parser.attach_value(), "aaavvv");
}

TEST(Parser, kCmdGet) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdGet));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdGet);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.table(), "test-table");
  ASSERT_EQ(parser.key(), "key_0");
}

TEST(Parser, kCmdDelete) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdDelete));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdDelete);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.table(), "test-table");
  ASSERT_EQ(parser.key(), "key_0");
}

TEST(Parser, kCmdLoad) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdLoad));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdLoad);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.table(), "test-table");
  ASSERT_EQ(parser.file(), "/tmp/vec.txt");
}

TEST(Parser, kCmdGetKNN) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdGetKNN));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdGetKNN);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.key(), "key_0");
  ASSERT_EQ(parser.table(), "test-table");
  ASSERT_EQ(parser.limit(), 20);
}

TEST(Parser, kCmdGetKNN2) {
  vectordb::Parser parser(vectordb::example_cmdstr(vectordb::kCmdGetKNN2));
  ASSERT_EQ(parser.cmd(), vectordb::kCmdGetKNN);
  std::cout << parser.ToJsonString(false, false) << std::endl;

  ASSERT_EQ(parser.table(), "test-table");
  ASSERT_EQ(parser.limit(), 20);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}