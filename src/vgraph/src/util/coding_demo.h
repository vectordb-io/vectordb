#ifndef VGRAPH_UTIL_CODING_DEMO_H_
#define VGRAPH_UTIL_CODING_DEMO_H_

#include <string>
#include <vector>

#include "coding.h"
#include "common.h"
#include <nlohmann/json.hpp>

namespace vgraph {

/**
 * @brief 用于演示复杂结构体的序列化和反序列化的测试结构体
 *
 * 包含多种基本数据类型
 * 使用coding.h中定义的编解码函数处理
 * - int8_t, 使用EncodeFixed8, DecodeFixed8
 * - uint8_t, 使用EncodeFixed8, DecodeFixed8
 * - int16_t, 使用EncodeFixed16, DecodeFixed16
 * - uint16_t, 使用EncodeFixed16, DecodeFixed16
 * - int32_t, 使用EncodeFixed32, DecodeFixed32
 * - uint32_t, 使用EncodeFixed32, DecodeFixed32
 * - int64_t, 使用EncodeFixed64, DecodeFixed64
 * - uint64_t, 使用EncodeFixed64和DecodeFixed64
 * - float, 使用EncodeFloat, DecodeFloat
 * - double, 使用EncodeDouble, DecodeDouble
 * - std::string, 使用EncodeString, DecodeString, EncodeString2, DecodeString2
 */
struct BaseType {
  int8_t i8;
  uint8_t u8;
  int16_t i16;
  uint16_t u16;
  int32_t i32;
  uint32_t u32;
  int64_t i64;
  uint64_t u64;
  float f;
  double d;
  std::string s;

  /**
   * @brief 计算序列化该结构体所需的最大字节数
   * @return 返回序列化所需的最大字节数
   * 
   * 计算方式包括：
   * - 固定长度类型的大小
   *   - int8_t, uint8_t: 1字节
   *   - int16_t, uint16_t: 2字节
   *   - int32_t, uint32_t: 4字节
   *   - int64_t, uint64_t: 8字节
   *   - float: 4字节
   *   - double: 8字节
   * - 变长类型
   *   - 字符串所占用的字节数: 前缀最大长度(5字节) + 内容长度
   */
  int32_t MaxBytes() const;

  /**
   * @brief 计算序列化该结构体所需的最小字节数
   * @return 返回序列化所需的最小字节数
   * 
   * 计算方式包括：
   * - 固定长度类型的大小
   *   - int8_t, uint8_t: 1字节
   *   - int16_t, uint16_t: 2字节
   *   - int32_t, uint32_t: 4字节
   *   - int64_t, uint64_t: 8字节
   *   - float: 4字节
   *   - double: 8字节
   * - 变长类型
   *   - 字符串所占用的字节数: 前缀最小长度(1字节) + 空字符串长度(0字节)
   */
  int32_t MinBytes() const;

  /**
   * @brief 将结构体转换为字符串, 确保序列化后的字符串可以被正确反序列化
   * @param output 输出字符串的引用
   * @return 成功返回处理的字节数, 失败返回-1
   */
  int32_t ToString(std::string &output) const;

  /**
   * @brief 从字符串中解析各个字段并填充结构体
   * @param input 输入字符串的引用
   * @return 成功返回处理的字节数, 失败返回-1
   */
  int32_t FromString(const std::string& input);

  /**
   * @brief 将结构体的所有成员序列化为字节紧凑的字符数组, 确保序列化后的字符数组可以被正确反序列化
   * @param output 输出数据的指针
   * @param len 输出数据的长度
   * @return 成功返回处理的字节数, 失败返回-1
   */
  int32_t ToBytes(char* output, int32_t len) const;

  /**
   * @brief 从字符数组解析各个字段并填充结构体
   * @param input 输入的字符数组指针
   * @param len 输入字符数组的长度
   * @return 成功返回处理的字节数, 失败返回-1
   */
  int32_t FromBytes(const char* input, int32_t len);

  /**
   * @brief 将结构体的所有成员序列化为JSON格式
   * @return 返回包含所有成员数据的JSON对象
   * 
   * JSON对象包含以下字段：
   * - i8: int8_t类型整数
   * - u8: uint8_t类型整数
   * - i16: int16_t类型整数
   * - u16: uint16_t类型整数
   * - i32: int32_t类型整数
   * - u32: uint32_t类型整数
   * - i64: int64_t类型整数
   * - u64: uint64_t类型整数
   * - f: float类型浮点数
   * - d: double类型浮点数
   * - s: string类型字符串
   */
  nlohmann::json ToJson() const;

  /**
   * @brief 将结构体的所有成员序列化为JSON字符串
   * @param pretty 是否格式化输出的JSON字符串，默认为false
   * @return 返回JSON格式的字符串
   * 
   * 当pretty为true时：
   * - 使用缩进和换行使输出更易读
   * - 缩进使用空格，缩进大小由JSON_TAB_SIZE定义
   * 
   * 当pretty为false时：
   * - 输出紧凑的单行JSON字符串
   * - 不包含多余的空格和换行
   */
  const std::string ToJsonString(bool pretty = false) const;
};

/**
 * @brief 用于演示数组类型序列化和反序列化的测试结构体
 *
 * 包含两种数组类型:
 * - std::vector<int32_t>: 32位整数数组
 * - std::vector<std::string>: 字符串数组
 * 使用coding.h中定义的编解码函数处理
 * - int8_t, 使用EncodeFixed8, DecodeFixed8
 * - uint8_t, 使用EncodeFixed8, DecodeFixed8
 * - int16_t, 使用EncodeFixed16, DecodeFixed16
 * - uint16_t, 使用EncodeFixed16, DecodeFixed16
 * - int32_t, 使用EncodeFixed32, DecodeFixed32
 * - uint32_t, 使用EncodeFixed32, DecodeFixed32
 * - int64_t, 使用EncodeFixed64, DecodeFixed64
 * - uint64_t, 使用EncodeFixed64和DecodeFixed64
 * - float, 使用EncodeFloat, DecodeFloat
 * - double, 使用EncodeDouble, DecodeDouble
 * - std::string, 使用EncodeString, DecodeString, EncodeString2, DecodeString2
 * 
 * 序列化格式：
 * - 整数数组: 使用定长编码存储数组长度，然后依次序列化每个整数，每个整数使用变长编码
 * - 字符串数组: 使用定长编码存储数组长度，然后依次序列化每个字符串，每个字符串使用变长编码
 */
struct ArrayType {
  std::vector<int32_t> array_i32;
  std::vector<std::string> array_str;

  /**
   * @brief 计算序列化该结构体所需的最大字节数
   * @return 返回序列化所需的最大字节数
   * 
   * 计算方式包括：
   * - 整数数组
   *   - 数组长度的定长编码(4字节)
   *   - 每个整数使用变长编码
   * - 字符串数组
   *   - 数组长度的定长编码(4字节)
   *   - 每个字符串的长度编码(最多5字节)和内容
   */
  int32_t MaxBytes() const;

  /**
   * @brief 计算序列化该结构体所需的最小字节数
   * @return 返回序列化所需的最小字节数
   * 
   * 计算方式包括：
   * - 整数数组
   *   - 空数组长度定长编码(4字节)
   * - 字符串数组
   *   - 空数组长度定长编码(4字节)
   */
  int32_t MinBytes() const;

  /**
   * @brief 将结构体转换为字符串, 确保序列化后的字符串可以被正确反序列化
   * @param output 输出字符串的引用
   * @return 成功返回处理的字节数, 失败返回-1
   */
  int32_t ToString(std::string &output) const;

  /**
   * @brief 从字符串中解析各个字段并填充结构体
   * @param input 输入字符串的引用
   * @return 成功返回处理的字节数, 失败返回-1
   */
  int32_t FromString(const std::string& input);

  /**
   * @brief 将结构体的所有成员序列化为字节紧凑的字符数组
   * @param output 输出数据的指针
   * @param len 输出数据的长度
   * @return 成功返回处理的字节数, 失败返回-1
   */
  int32_t ToBytes(char* output, int32_t len) const;

  /**
   * @brief 从字符数组解析各个字段并填充结构体
   * @param input 输入的字符数组指针
   * @param len 输入字符数组的长度
   * @return 成功返回处理的字节数, 失败返回-1
   */
  int32_t FromBytes(const char* input, int32_t len);

  /**
   * @brief 将结构体的所有成员序列化为JSON格式
   * @return 返回包含所有成员数据的JSON对象
   * 
   * JSON对象包含以下字段：
   * - array_i32: 整数数组
   * - array_str: 字符串数组
   */
  nlohmann::json ToJson() const;

  /**
   * @brief 将结构体的所有成员序列化为JSON字符串
   * @param pretty 是否格式化输出的JSON字符串，默认为false
   * @return 返回JSON格式的字符串
   * 
   * 当pretty为true时：
   * - 使用缩进和换行使输出更易读
   * - 缩进使用空格，缩进大小由JSON_TAB_SIZE定义
   * 
   * 当pretty为false时：
   * - 输出紧凑的单行JSON字符串
   * - 不包含多余的空格和换行
   */
  const std::string ToJsonString(bool pretty = false) const;
};

}  // namespace vgraph

#endif  // VGRAPH_UTIL_CODING_DEMO_H_
    