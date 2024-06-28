#include "append_entries.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "util.h"

TEST(AppendEntries, test) {
  uint32_t ip32;
  bool b = vraft::StringToIpU32("127.0.0.1", ip32);
  assert(b);

  vraft::RaftAddr src(ip32, 1234, 55);
  vraft::RaftAddr dest(ip32, 5678, 99);

  vraft::AppendEntries msg;
  msg.src = src;
  msg.dest = dest;
  msg.term = 77;
  msg.uid = vraft::UniqId(&msg);
  msg.pre_log_index = 88;
  msg.pre_log_term = 99;
  msg.commit_index = 100;
  for (int32_t i = 1; i <= 5; ++i) {
    vraft::LogEntry entry;
    entry.index = i;
    entry.pre_chk_all = 0;
    entry.CheckSum();
    entry.append_entry.term = i * 2;
    entry.append_entry.type = vraft::kData;
    char buf[32];
    snprintf(buf, sizeof(buf), "value_%d", i);
    entry.append_entry.value.append(buf);
    msg.entries.push_back(entry);
  }

  std::string msg_str;
  int32_t bytes = msg.ToString(msg_str);
  std::cout << "bytes:" << bytes << std::endl;

  std::cout << "encoding:" << std::endl;
  std::cout << msg.ToJsonString(true, true) << std::endl;
  std::cout << msg.ToJsonString(true, false) << std::endl;
  std::cout << msg.ToJsonString(false, true) << std::endl;
  std::cout << msg.ToJsonString(false, false) << std::endl;

  vraft::AppendEntries msg2;
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
  ASSERT_EQ(msg.pre_log_index, msg2.pre_log_index);
  ASSERT_EQ(msg.pre_log_term, msg2.pre_log_term);
  ASSERT_EQ(msg.commit_index, msg2.commit_index);
  for (size_t i = 0; i < msg.entries.size(); ++i) {
    ASSERT_EQ(msg.entries[i].index, msg2.entries[i].index);
    ASSERT_EQ(msg.entries[i].append_entry.term,
              msg2.entries[i].append_entry.term);
    ASSERT_EQ(msg.entries[i].append_entry.value,
              msg2.entries[i].append_entry.value);
  }
}

TEST(AppendEntries, test2) {
  uint32_t ip32;
  bool b = vraft::StringToIpU32("127.0.0.1", ip32);
  assert(b);

  vraft::RaftAddr src(ip32, 1234, 55);
  vraft::RaftAddr dest(ip32, 5678, 99);

  vraft::AppendEntries msg;
  msg.src = src;
  msg.dest = dest;
  msg.term = 77;
  msg.uid = vraft::UniqId(&msg);
  msg.pre_log_index = 88;
  msg.pre_log_term = 99;
  msg.commit_index = 100;

  std::string msg_str;
  int32_t bytes = msg.ToString(msg_str);
  std::cout << "bytes:" << bytes << std::endl;

  std::cout << "encoding:" << std::endl;
  std::cout << msg.ToJsonString(true, true) << std::endl;
  std::cout << msg.ToJsonString(true, false) << std::endl;
  std::cout << msg.ToJsonString(false, true) << std::endl;
  std::cout << msg.ToJsonString(false, false) << std::endl;

  vraft::AppendEntries msg2;
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
  ASSERT_EQ(msg.pre_log_index, msg2.pre_log_index);
  ASSERT_EQ(msg.pre_log_term, msg2.pre_log_term);
  ASSERT_EQ(msg.commit_index, msg2.commit_index);
  ASSERT_EQ(msg.entries.size(), static_cast<size_t>(0));
  ASSERT_EQ(msg2.entries.size(), static_cast<size_t>(0));
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}