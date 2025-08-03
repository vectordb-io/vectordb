#include <gtest/gtest.h>
#include "coding_demo.h"

namespace vgraph {

// 测试基本类型的序列化和反序列化
TEST(TestStructTest, BasicTypeTest) {
  BaseType base;
  // 设置测试数据
  base.i8 = -8;
  base.u8 = 8;
  base.i16 = -16;
  base.u16 = 16;
  base.i32 = -32;
  base.u32 = 32;
  base.i64 = -64;
  base.u64 = 64;
  base.f = 3.14f;
  base.d = 3.1415926;
  base.s = "test string";

  // 测试ToString和FromString
  std::string output;
  ASSERT_GT(base.ToString(output), 0);
  
  BaseType decoded;
  ASSERT_GT(decoded.FromString(output), 0);
  
  // 验证解码后的值是否正确
  EXPECT_EQ(decoded.i8, base.i8);
  EXPECT_EQ(decoded.u8, base.u8);
  EXPECT_EQ(decoded.i16, base.i16);
  EXPECT_EQ(decoded.u16, base.u16);
  EXPECT_EQ(decoded.i32, base.i32);
  EXPECT_EQ(decoded.u32, base.u32);
  EXPECT_EQ(decoded.i64, base.i64);
  EXPECT_EQ(decoded.u64, base.u64);
  EXPECT_FLOAT_EQ(decoded.f, base.f);
  EXPECT_DOUBLE_EQ(decoded.d, base.d);
  EXPECT_EQ(decoded.s, base.s);
}

// 测试空字符串的情况
TEST(TestStructTest, EmptyStringTest) {
  BaseType base;
  base.i8 = 1;
  base.u8 = 2;
  base.i16 = 3;
  base.u16 = 4;
  base.i32 = 5;
  base.u32 = 6;
  base.i64 = 7;
  base.u64 = 8;
  base.f = 9.0f;
  base.d = 10.0;
  base.s = "";  // 空字符串

  std::string output;
  ASSERT_GT(base.ToString(output), 0);
  
  BaseType decoded;
  ASSERT_GT(decoded.FromString(output), 0);
  
  EXPECT_EQ(decoded.s, "");
  EXPECT_EQ(decoded.i8, base.i8);
  // ... 验证其他字段
}

// 测试错误情况
TEST(TestStructTest, ErrorTest) {
  BaseType base;
  
  // 测试空输入
  EXPECT_EQ(base.FromString(""), -1);
  
  // 测试输入数据太短
  std::string short_input(1, 'x');
  EXPECT_EQ(base.FromString(short_input), -1);
  
  // 测试ToBytes的空指针
  EXPECT_EQ(base.ToBytes(nullptr, 100), -1);
  
  // 测试ToBytes的长度为0
  char buf[100];
  EXPECT_EQ(base.ToBytes(buf, 0), -1);
  
  // 测试ToBytes的缓冲区太小
  EXPECT_EQ(base.ToBytes(buf, 1), -1);
}

// 测试MinBytes和MaxBytes
TEST(TestStructTest, BytesCalculationTest) {
  BaseType base;
  base.s = "test";
  
  // MinBytes应该是固定的
  EXPECT_EQ(base.MinBytes(), 
            sizeof(int8_t) + sizeof(uint8_t) +  // 8位整数
            sizeof(int16_t) + sizeof(uint16_t) +  // 16位整数
            sizeof(int32_t) + sizeof(uint32_t) +  // 32位整数
            sizeof(int64_t) + sizeof(uint64_t) +  // 64位整数
            sizeof(float) + sizeof(double) +  // 浮点数
            1);  // 空字符串的最小长度(1字节)

  // MaxBytes应该考虑字符串长度
  EXPECT_EQ(base.MaxBytes(),
            sizeof(int8_t) + sizeof(uint8_t) +  // 8位整数
            sizeof(int16_t) + sizeof(uint16_t) +  // 16位整数
            sizeof(int32_t) + sizeof(uint32_t) +  // 32位整数
            sizeof(int64_t) + sizeof(uint64_t) +  // 64位整数
            sizeof(float) + sizeof(double) +  // 浮点数
            5 + base.s.length());  // 字符串长度前缀(5字节) + 字符串内容
}

// 测试极限值
TEST(TestStructTest, BoundaryValueTest) {
  BaseType base;
  base.i8 = INT8_MIN;
  base.u8 = UINT8_MAX;
  base.i16 = INT16_MIN;
  base.u16 = UINT16_MAX;
  base.i32 = INT32_MIN;
  base.u32 = UINT32_MAX;
  base.i64 = INT64_MIN;
  base.u64 = UINT64_MAX;
  base.f = std::numeric_limits<float>::max();
  base.d = std::numeric_limits<double>::max();
  base.s = std::string(1000, 'x');  // 长字符串

  std::string output;
  ASSERT_GT(base.ToString(output), 0);
  
  BaseType decoded;
  ASSERT_GT(decoded.FromString(output), 0);
  
  EXPECT_EQ(decoded.i8, INT8_MIN);
  EXPECT_EQ(decoded.u8, UINT8_MAX);
  EXPECT_EQ(decoded.i16, INT16_MIN);
  EXPECT_EQ(decoded.u16, UINT16_MAX);
  EXPECT_EQ(decoded.i32, INT32_MIN);
  EXPECT_EQ(decoded.u32, UINT32_MAX);
  EXPECT_EQ(decoded.i64, INT64_MIN);
  EXPECT_EQ(decoded.u64, UINT64_MAX);
  EXPECT_FLOAT_EQ(decoded.f, std::numeric_limits<float>::max());
  EXPECT_DOUBLE_EQ(decoded.d, std::numeric_limits<double>::max());
  EXPECT_EQ(decoded.s, std::string(1000, 'x'));
}

TEST(TestStructTest, MyTest) {
  BaseType base;
  // 设置测试数据
  base.i8 = -8;
  base.u8 = 8;
  base.i16 = -16;
  base.u16 = 16;
  base.i32 = -32;
  base.u32 = 32;
  base.i64 = -64;
  base.u64 = 64;
  base.f = 3.14f;
  base.d = 3.1415926;
  base.s = "test string";

  std::cout << base.ToJsonString() << std::endl;
  std::cout << base.ToJsonString(true) << std::endl;

  ArrayType arr;
  arr.array_i32 = {1, 2, 3};
  arr.array_str = {"hello", "world"};
  std::cout << arr.ToJsonString() << std::endl;
  std::cout << arr.ToJsonString(true) << std::endl;
}

// 测试数组类型的基本序列化和反序列化
TEST(TestStructTest, ArrayTypeBasicTest) {
  ArrayType arr;
  // 设置测试数据
  arr.array_i32 = {-1, 0, 1, 100, -100, INT32_MAX, INT32_MIN};
  arr.array_str = {"hello", "", "world", "测试中文", "!@#$%"};

  // 测试ToString和FromString
  std::string output;
  ASSERT_GT(arr.ToString(output), 0);
  
  ArrayType decoded;
  ASSERT_GT(decoded.FromString(output), 0);
  
  // 验证解码后的值是否正确
  EXPECT_EQ(decoded.array_i32, arr.array_i32);
  EXPECT_EQ(decoded.array_str, arr.array_str);
}

// 测试空数组的情况
TEST(TestStructTest, EmptyArrayTest) {
  ArrayType arr;
  // 两个数组都为空

  std::string output;
  ASSERT_GT(arr.ToString(output), 0);
  
  ArrayType decoded;
  ASSERT_GT(decoded.FromString(output), 0);
  
  EXPECT_TRUE(decoded.array_i32.empty());
  EXPECT_TRUE(decoded.array_str.empty());
}

// 测试数组相关的错误情况
TEST(TestStructTest, ArrayErrorTest) {
  ArrayType arr;
  
  // 测试空输入
  EXPECT_EQ(arr.FromString(""), -1);
  
  // 测试输入数据太短
  std::string short_input(1, 'x');
  EXPECT_EQ(arr.FromString(short_input), -1);
  
  // 测试ToBytes的空指针
  EXPECT_EQ(arr.ToBytes(nullptr, 100), -1);
  
  // 测试ToBytes的长度为0
  char buf[100];
  EXPECT_EQ(arr.ToBytes(buf, 0), -1);
  
  // 测试ToBytes的缓冲区太小
  EXPECT_EQ(arr.ToBytes(buf, 1), -1);
}

// 测试数组的MinBytes和MaxBytes
TEST(TestStructTest, ArrayBytesCalculationTest) {
  ArrayType arr;
  
  // MinBytes应该是两个空数组的长度编码
  EXPECT_EQ(arr.MinBytes(), sizeof(int32_t) * 2);  // 两个4字节的长度字段

  // 添加一些数据
  arr.array_i32 = {1, 2, 3};
  arr.array_str = {"test1", "test2"};
  
  // MaxBytes应该考虑:
  // 1. 整数数组: 4字节长度 + 每个整数最多5字节
  // 2. 字符串数组: 4字节长度 + 每个字符串(5字节长度 + 内容)
  int32_t expected_max = 
      sizeof(int32_t) +  // 整数数组长度
      arr.array_i32.size() * 5 +  // 整数数组内容
      sizeof(int32_t);  // 字符串数组长度
  
  // 加上每个字符串的最大长度
  for (const auto& str : arr.array_str) {
    expected_max += 5 + str.length();  // 每个字符串的长度前缀和内容
  }
  
  EXPECT_EQ(arr.MaxBytes(), expected_max);
}

// 测试大数组
TEST(TestStructTest, LargeArrayTest) {
  ArrayType arr;
  
  // 生成大量整数
  for (int i = 0; i < 1000; i++) {
    arr.array_i32.push_back(i);
  }
  
  // 生成大量字符串
  for (int i = 0; i < 100; i++) {
    arr.array_str.push_back(std::string(100, 'a' + (i % 26)));
  }

  std::string output;
  ASSERT_GT(arr.ToString(output), 0);
  
  ArrayType decoded;
  ASSERT_GT(decoded.FromString(output), 0);
  
  EXPECT_EQ(decoded.array_i32, arr.array_i32);
  EXPECT_EQ(decoded.array_str, arr.array_str);
}

// 测试JSON序列化
TEST(TestStructTest, ArrayJsonTest) {
  ArrayType arr;
  arr.array_i32 = {-1, 0, 1, 100};
  arr.array_str = {"hello", "world"};

  // 测试普通JSON序列化
  std::string json_str = arr.ToJsonString();
  EXPECT_FALSE(json_str.empty());
  EXPECT_NE(json_str.find("array_i32"), std::string::npos);
  EXPECT_NE(json_str.find("array_str"), std::string::npos);

  // 测试美化的JSON序列化
  std::string pretty_json = arr.ToJsonString(true);
  EXPECT_FALSE(pretty_json.empty());
  EXPECT_NE(pretty_json.find("\n"), std::string::npos);  // 应该包含换行符
  EXPECT_NE(pretty_json.find("  "), std::string::npos);  // 应该包含缩进
}

}  // namespace vgraph
