#include "coding.h"

#include <gtest/gtest.h>

#include <iostream>

TEST(CODING, DecodeVarint32) {
  uint32_t u32 = 38;

  char buf[sizeof(u32)];
  char *p = vraft::EncodeVarint32(buf, u32);
  EXPECT_GT(p - buf, 0);
  std::cout << u32 << std::endl;

  vraft::Slice sls2(buf, sizeof(buf));
  uint32_t u32_2;
  bool b = vraft::DecodeVarint32(&sls2, &u32_2);
  EXPECT_EQ(u32, u32_2);
  EXPECT_EQ(b, true);
  std::cout << u32_2 << std::endl;

  vraft::Slice sls3(buf, sizeof(buf));
  uint32_t u32_3;
  int32_t bytes = vraft::DecodeVarint32Bytes(&sls3, &u32_3);
  EXPECT_EQ(u32, u32_3);
  EXPECT_EQ(bytes, p - buf);
  std::cout << u32_3 << std::endl;
  std::cout << "bytes:" << bytes << std::endl;
}

TEST(CODING, DecodeString) {
  std::string src_str("abcd");
  vraft::Slice sls(src_str.c_str(), src_str.size());
  std::cout << src_str << std::endl;

  std::string str;
  vraft::EncodeString(&str, sls);

  vraft::Slice sls_input(str.c_str(), str.size());
  vraft::Slice result;
  bool b = DecodeString(&sls_input, &result);
  std::string result_str = result.ToString();
  std::cout << result_str << std::endl;
  EXPECT_EQ(b, true);
  EXPECT_EQ(src_str, result_str);

  vraft::Slice sls_input2(str.c_str(), str.size());
  vraft::Slice result2;
  int32_t bytes = DecodeString2(&sls_input2, &result2);
  std::string result_str2 = result2.ToString();
  std::cout << "bytes:" << bytes << std::endl;
  std::cout << result_str2 << std::endl;
  EXPECT_GT(bytes, 0);
  EXPECT_EQ(src_str, result_str2);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}