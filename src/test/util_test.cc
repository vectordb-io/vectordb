#include "util.h"

#include <gtest/gtest.h>

#include <iostream>

#include "simple_random.h"

TEST(Split, Split) {
  {
    std::vector<std::string> result;
    vraft::Split("aaa   bb cc", ' ', result);
    for (auto &s : result) {
      std::cout << s << std::endl;
    }
    ASSERT_EQ(result.size(), (size_t)3);
    ASSERT_EQ(result[0], "aaa");
    ASSERT_EQ(result[1], "bb");
    ASSERT_EQ(result[2], "cc");
  }

  {
    std::vector<std::string> result;
    vraft::Split("aaa bb cc", ' ', result);
    for (auto &s : result) {
      std::cout << s << std::endl;
    }
    ASSERT_EQ(result.size(), (size_t)3);
    ASSERT_EQ(result[0], "aaa");
    ASSERT_EQ(result[1], "bb");
    ASSERT_EQ(result[2], "cc");
  }

  {
    std::vector<std::string> result;
    vraft::Split("  aaabb cc", ' ', result);
    for (auto &s : result) {
      std::cout << s << std::endl;
    }
    ASSERT_EQ(result.size(), (size_t)2);
    ASSERT_EQ(result[0], "aaabb");
    ASSERT_EQ(result[1], "cc");
  }

  {
    std::vector<std::string> result;
    vraft::Split("aaabb cc    ", ' ', result);
    for (auto &s : result) {
      std::cout << s << std::endl;
    }
    ASSERT_EQ(result.size(), (size_t)2);
    ASSERT_EQ(result[0], "aaabb");
    ASSERT_EQ(result[1], "cc");
  }
}

TEST(SplitTest, HandlesSingleSeparator) {
  std::vector<std::string> result;
  vraft::Split("aaa bb cc", ' ', result);

  std::vector<std::string> expected = {"aaa", "bb", "cc"};
  EXPECT_EQ(result, expected);
}

TEST(SplitTest, HandlesMultipleSeparators) {
  std::vector<std::string> result;
  vraft::Split("aaa   bb cc", ' ', result);

  std::vector<std::string> expected = {"aaa", "bb", "cc"};
  EXPECT_EQ(result, expected);
}

TEST(SplitTest, HandlesLeadingSeparators) {
  std::vector<std::string> result;
  vraft::Split("  aaa bb cc", ' ', result);

  std::vector<std::string> expected = {"aaa", "bb", "cc"};
  EXPECT_EQ(result, expected);
}

TEST(SplitTest, HandlesTrailingSeparators) {
  std::vector<std::string> result;
  vraft::Split("aaa bb cc  ", ' ', result);

  std::vector<std::string> expected = {"aaa", "bb", "cc"};
  EXPECT_EQ(result, expected);
}

TEST(SplitTest, HandlesNoSeparators) {
  std::vector<std::string> result;
  vraft::Split("aaabbcc", ' ', result);

  std::vector<std::string> expected = {"aaabbcc"};
  EXPECT_EQ(result, expected);
}

TEST(SplitTest, HandlesEmptyString) {
  std::vector<std::string> result;
  vraft::Split("", ' ', result);

  std::vector<std::string> expected = {};
  EXPECT_EQ(result, expected);
}

TEST(UniqId, test) {
  char buf[20];
  for (int i = 0; i < 20; ++i) {
    printf("0x%X \n", vraft::UniqId(&buf[i]));
  }
}

TEST(SimpleRandom, test) {
  vraft::SimpleRandom r(150, 300);
  for (int i = 0; i < 10; ++i) {
    uint32_t n = r.Get();
    std::cout << n << " ";
    EXPECT_LE(static_cast<uint32_t>(150), n);
    EXPECT_GE(static_cast<uint32_t>(300), n);
  }
  std::cout << std::endl;
}

TEST(UTIL, MISC) { std::cout << vraft::IpU32ToIpString(32765) << std::endl; }

TEST(UTIL, ToLower) {
  std::string s = "AAAaaa";
  vraft::ToLower(s);
  EXPECT_EQ(s, std::string("aaaaaa"));
}

TEST(UTIL, HostNameToIpU32) {
  uint32_t ip32;
  bool b = vraft::HostNameToIpU32("baidu.com", ip32);
  EXPECT_EQ(b, true);
  std::cout << ip32 << std::endl;

  std::string ip_str = vraft::IpU32ToIpString(ip32);
  std::cout << ip_str << std::endl;
  // EXPECT_EQ(ip_str, std::string("110.242.68.66"));
}

TEST(UTIL, IpStringToIpU32) {
  uint32_t ip32;
  bool b = vraft::IpStringToIpU32("1.2.3.4", ip32);
  EXPECT_EQ(b, true);
  std::cout << ip32 << std::endl;

  std::string ip_str = vraft::IpU32ToIpString(ip32);
  std::cout << ip_str << std::endl;
  EXPECT_EQ(ip_str, std::string("1.2.3.4"));
}

TEST(UTIL, StringToIpU32) {
  uint32_t ip32;
  bool b = vraft::StringToIpU32("baidu.com", ip32);
  EXPECT_EQ(b, true);
  std::cout << ip32 << std::endl;

  std::string ip_str = vraft::IpU32ToIpString(ip32);
  std::cout << ip_str << std::endl;
  // EXPECT_EQ(ip_str, std::string("110.242.68.66"));
}

TEST(UTIL, PointerToHexStr) {
  int a;
  int *p = &a;
  std::cout << vraft::PointerToHexStr(p) << std::endl;
}

TEST(UTIL, StrToHexStr) {
  std::string s = "abc";
  std::cout << vraft::StrToHexStr(s.c_str(), s.size()) << std::endl;
}

TEST(UTIL, ConvertStringToArgcArgv) {
  std::string input = "example command with multiple arguments";

  int argc;
  char **argv;
  vraft::ConvertStringToArgcArgv(input, &argc, &argv);

  // For testing: Print the arguments to verify correctness.
  for (int i = 0; i < argc; ++i) {
    std::cout << argv[i] << std::endl;
  }

  // Whenever done with argv, free the allocated memory.
  vraft::FreeArgv(argc, argv);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}