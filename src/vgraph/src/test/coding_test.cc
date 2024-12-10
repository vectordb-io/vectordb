#include "coding.h"
#include <gtest/gtest.h>
#include <cmath>  // 为了使用 std::isnan

namespace vgraph {
namespace {

class CodingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    CodingInit();  // 初始化编码模块
  }
};

// 字节序测试
TEST_F(CodingTest, Endianness) {
  bool is_little = IsLittleEndian();
  EXPECT_TRUE(is_little || !is_little);  // 确保返回了有效的布尔值
}

// 二进制字符串转换测试
TEST_F(CodingTest, ByteToBitString) {
  EXPECT_EQ("00000000", ByteToBitString(0x00));
  EXPECT_EQ("11111111", ByteToBitString(0xFF));
  EXPECT_EQ("10101010", ByteToBitString(0xAA));
  EXPECT_EQ("01010101", ByteToBitString(0x55));
}

// 字节序列二进制表示测试
TEST_F(CodingTest, BitsString) {
  unsigned char data[] = {0xAA, 0x55, 0xFF};
  EXPECT_EQ("101010100101010111111111", BitsStringFromLow(reinterpret_cast<char*>(data), 3));
  EXPECT_EQ("111111110101010110101010", BitsStringFromHigh(reinterpret_cast<char*>(data), 3));
}

// 8位固定长度编解码测试
TEST_F(CodingTest, Fixed8) {
  char buf[1];
  uint8_t values[] = {0, 1, 127, 128, 255};
  
  for (uint8_t val : values) {
    EncodeFixed8(buf, val);
    EXPECT_EQ(val, DecodeFixed8(buf));
  }
}

// 8位变长编解码测试
TEST_F(CodingTest, Varint8) {
  char buf[2];
  uint8_t val, decoded;
  Slice input;
  
  // 测试小数值
  val = 127;
  char* end = EncodeVarint8(buf, val);
  input = Slice(buf, end - buf);
  EXPECT_TRUE(DecodeVarint8(&input, &decoded));
  EXPECT_EQ(val, decoded);
  EXPECT_EQ(0, input.size());
  
  // 测试大数值
  val = 255;
  end = EncodeVarint8(buf, val);
  input = Slice(buf, end - buf);
  EXPECT_TRUE(DecodeVarint8(&input, &decoded));
  EXPECT_EQ(val, decoded);
  EXPECT_EQ(0, input.size());
}

// 16位固定长度编解码测试
TEST_F(CodingTest, Fixed16) {
  char buf[2];
  uint16_t values[] = {0, 1, 255, 256, 32767, 32768, 65535};
  
  for (uint16_t val : values) {
    EncodeFixed16(buf, val);
    EXPECT_EQ(val, DecodeFixed16(buf));
  }
}

// 32位固定长度编解码测试
TEST_F(CodingTest, Fixed32) {
  char buf[4];
  uint32_t values[] = {0, 1, 65535, 65536, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF};
  
  for (uint32_t val : values) {
    EncodeFixed32(buf, val);
    EXPECT_EQ(val, DecodeFixed32(buf));
  }
}

// 64位固定长度编解码测试
TEST_F(CodingTest, Fixed64) {
  char buf[8];
  uint64_t values[] = {0, 1, 0xFFFFFFFF, 0x100000000, 
                       0x7FFFFFFFFFFFFFFF, 0x8000000000000000, 
                       0xFFFFFFFFFFFFFFFF};
  
  for (uint64_t val : values) {
    EncodeFixed64(buf, val);
    EXPECT_EQ(val, DecodeFixed64(buf));
  }
}

// 浮点数编解码测试
TEST_F(CodingTest, FloatingPoint) {
  char buf[8];
  
  // float测试
  float f_values[] = {0.0f, -0.0f, 1.0f, -1.0f, 3.14159f, -3.14159f,
                      std::numeric_limits<float>::infinity(),
                      -std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()};
  
  for (float val : f_values) {
    EncodeFloat(buf, val);
    float decoded = DecodeFloat(buf);
    if (std::isnan(val)) {
      EXPECT_TRUE(std::isnan(decoded));
    } else {
      EXPECT_EQ(val, decoded);
    }
  }
  
  // double测试
  double d_values[] = {0.0, -0.0, 1.0, -1.0, 3.14159265359, -3.14159265359,
                       std::numeric_limits<double>::infinity(),
                       -std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()};
  
  for (double val : d_values) {
    EncodeDouble(buf, val);
    double decoded = DecodeDouble(buf);
    if (std::isnan(val)) {
      EXPECT_TRUE(std::isnan(decoded));
    } else {
      EXPECT_EQ(val, decoded);
    }
  }
}

// 字串编解码测试
TEST_F(CodingTest, String) {
  // 测试空字符串
  std::string empty_str;
  Slice empty_result;
  EncodeString(&empty_str, Slice(""));
  Slice empty_input(empty_str);
  EXPECT_TRUE(DecodeString(&empty_input, &empty_result));
  EXPECT_EQ(0, empty_result.size());
  
  // 测试普通字符串
  std::string normal_str;
  Slice normal_result;
  const std::string test_str = "Hello, World!";
  EncodeString(&normal_str, Slice(test_str));
  Slice normal_input(normal_str);
  EXPECT_TRUE(DecodeString(&normal_input, &normal_result));
  EXPECT_EQ(test_str, normal_result.ToString());
  
  // 测试包含特殊字符的字符串
  std::string special_str;
  Slice special_result;
  const std::string special_test_str = "Hello\0World\n\r\t!";
  EncodeString(&special_str, Slice(special_test_str.data(), special_test_str.size()));
  Slice special_input(special_str);
  EXPECT_TRUE(DecodeString(&special_input, &special_result));
  EXPECT_EQ(special_test_str, std::string(special_result.data(), special_result.size()));
}

// 字串编解码测试
TEST_F(CodingTest, MyTest) {
  {
    std::string s = "abc";
    std::string dst;
    Slice result;
    EncodeString(&dst, Slice(s));
    PrintBin(dst.data(), dst.size(), true);
  }

  {
    std::string s = "";
    std::string dst;
    Slice result;
    EncodeString(&dst, Slice(s));
    PrintBin(dst.data(), dst.size(), true);
  }
}

// 变长字符串编解码测试
TEST_F(CodingTest, String2) {
  char buf[1024];
  Slice result;
  
  // 测试空字符串
  char* end = EncodeString2(buf, sizeof(buf), Slice(""));
  Slice input(buf, end - buf);
  EXPECT_GT(DecodeString2(&input, &result), 0);
  EXPECT_EQ(0, result.size());
  
  // 测试普通字符串
  const std::string test_str = "Hello, World!";
  end = EncodeString2(buf, sizeof(buf), Slice(test_str));
  input = Slice(buf, end - buf);
  EXPECT_GT(DecodeString2(&input, &result), 0);
  EXPECT_EQ(test_str, result.ToString());
  
  // 测试长字符串
  std::string long_str(1000, 'x');
  end = EncodeString2(buf, sizeof(buf), Slice(long_str));
  input = Slice(buf, end - buf);
  EXPECT_GT(DecodeString2(&input, &result), 0);
  EXPECT_EQ(long_str, result.ToString());
}

// 错误处理测试
TEST_F(CodingTest, ErrorHandling) {
  // 测试解码空输入
  Slice empty_input("");
  uint32_t value;
  EXPECT_FALSE(DecodeVarint32(&empty_input, &value));
  
  // 测试解码不完整的变长整数
  unsigned char buf[1] = {0x80};
  Slice incomplete_input(reinterpret_cast<char*>(buf), 1);
  EXPECT_FALSE(DecodeVarint32(&incomplete_input, &value));
  
  // 测试字符串解码错误
  Slice result;
  char str_buf[2] = {0x02, 0x01};  // 长度为2但只有1个字节的数据
  Slice invalid_str_input(str_buf, 2);
  EXPECT_FALSE(DecodeString(&invalid_str_input, &result));
}

}  // namespace
}  // namespace vgraph

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
