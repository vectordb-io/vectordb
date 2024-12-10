#include "coding_demo.h"

namespace vgraph {

int32_t BaseType::MaxBytes() const {
  // 计算固定长度类型的大小
  int32_t fixed_size = 
      sizeof(i8) +    // int8_t
      sizeof(u8) +    // uint8_t
      sizeof(i16) +   // int16_t
      sizeof(u16) +   // uint16_t
      sizeof(i32) +   // int32_t
      sizeof(u32) +   // uint32_t
      sizeof(i64) +   // int64_t
      sizeof(u64) +   // uint64_t
      sizeof(f) +     // float
      sizeof(d);      // double

  // 计算字符串的最大长度
  // 变长整数编码最多需要5个字节来存储长度
  int32_t str_size = 5 + s.length();  

  return fixed_size + str_size;
}

int32_t BaseType::MinBytes() const {
  // 计算固定长度类型的大小
  int32_t fixed_size = 
      sizeof(i8) +    // int8_t: 1字节
      sizeof(u8) +    // uint8_t: 1字节
      sizeof(i16) +   // int16_t: 2字节
      sizeof(u16) +   // uint16_t: 2字节
      sizeof(i32) +   // int32_t: 4字节
      sizeof(u32) +   // uint32_t: 4字节
      sizeof(i64) +   // int64_t: 8字节
      sizeof(u64) +   // uint64_t: 8字节
      sizeof(f) +     // float: 4字节
      sizeof(d);      // double: 8字节

  // 字符串的最小长度：前缀长度(1字节) + 空字符串长度(0字节)
  int32_t str_size = 1;  // 变长整数编码最小需要1个字节来存储长度

  return fixed_size + str_size;
}

int32_t BaseType::ToString(std::string& output) const {
  // 预分配足够的空间，避免多次内存分配
  output.resize(MaxBytes());
  
  // 直接调用ToBytes，因为std::string内部是连续的字符数组
  int32_t bytes = ToBytes(&output[0], output.size());
  if (bytes < 0) {
    return -1;
  }
  
  // 调整字符串大小为实际使用的长度
  output.resize(bytes);
  return bytes;
}

int32_t BaseType::FromString(const std::string& input) {
  if (input.empty()) {
    return -1;
  }
  
  // 直接调用FromBytes，因为std::string内部是连续的字符数组
  return FromBytes(input.data(), input.length());
}

int32_t BaseType::ToBytes(char* output, int32_t len) const {
  if (output == nullptr || len <= 0) {
    return -1;
  }
  
  // 检查输出缓冲区长度是否足够
  if (len < MaxBytes()) {
    return -1;
  }
  
  int32_t total_bytes = 0;
  
  // 8位整数
  EncodeFixed8(output + total_bytes, i8);
  total_bytes += sizeof(i8);
  
  EncodeFixed8(output + total_bytes, u8);
  total_bytes += sizeof(u8);
  
  // 16位整数
  EncodeFixed16(output + total_bytes, i16);
  total_bytes += sizeof(i16);
  
  EncodeFixed16(output + total_bytes, u16);
  total_bytes += sizeof(u16);
  
  // 32位整数
  EncodeFixed32(output + total_bytes, i32);
  total_bytes += sizeof(i32);
  
  EncodeFixed32(output + total_bytes, u32);
  total_bytes += sizeof(u32);
  
  // 64位整数
  EncodeFixed64(output + total_bytes, i64);
  total_bytes += sizeof(i64);
  
  EncodeFixed64(output + total_bytes, u64);
  total_bytes += sizeof(u64);
  
  // 浮点数
  EncodeFloat(output + total_bytes, f);
  total_bytes += sizeof(f);
  
  EncodeDouble(output + total_bytes, d);
  total_bytes += sizeof(d);
  
  // 字符串
  char* str_end = EncodeString2(output + total_bytes, len - total_bytes, Slice(s));
  if (str_end == nullptr) {
    return -1;
  }
  int32_t bytes = str_end - (output + total_bytes);
  total_bytes += bytes;
  
  return total_bytes;
}

int32_t BaseType::FromBytes(const char* input, int32_t len) {
  if (input == nullptr || len <= 0) {
    return -1;
  }
  
  int32_t total_bytes = 0;
  
  // 检查输入长度是否足够存储所有固定长度字段
  if (len < MinBytes()) {
    return -1;  // 输入数据太短
  }
  
  // 8位整数
  i8 = DecodeFixed8(input + total_bytes);
  total_bytes += sizeof(i8);
  
  u8 = DecodeFixed8(input + total_bytes);
  total_bytes += sizeof(u8);
  
  // 16位整数
  i16 = DecodeFixed16(input + total_bytes);
  total_bytes += sizeof(i16);
  
  u16 = DecodeFixed16(input + total_bytes);
  total_bytes += sizeof(u16);
  
  // 32位整数
  i32 = DecodeFixed32(input + total_bytes);
  total_bytes += sizeof(i32);
  
  u32 = DecodeFixed32(input + total_bytes);
  total_bytes += sizeof(u32);
  
  // 64位整数
  i64 = DecodeFixed64(input + total_bytes);
  total_bytes += sizeof(i64);
  
  u64 = DecodeFixed64(input + total_bytes);
  total_bytes += sizeof(u64);
  
  // 浮点数
  f = DecodeFloat(input + total_bytes);
  total_bytes += sizeof(f);
  
  d = DecodeDouble(input + total_bytes);
  total_bytes += sizeof(d);
  
  // 字符串
  Slice remaining(input + total_bytes, len - total_bytes);
  Slice str_slice;
  int32_t bytes = DecodeString2(&remaining, &str_slice);
  if (bytes < 0) {
    return -1;
  }
  s = str_slice.ToString();
  total_bytes += bytes;
  
  return total_bytes;
}

nlohmann::json BaseType::ToJson() const {
  nlohmann::json j;
  
  // 将所有成员序列化为JSON对象
  j["i8"] = i8;
  j["u8"] = u8;
  j["i16"] = i16;
  j["u16"] = u16;
  j["i32"] = i32;
  j["u32"] = u32;
  j["i64"] = i64;
  j["u64"] = u64;
  j["f"] = f;
  j["d"] = d;
  j["s"] = s;
  
  return j;
}

const std::string BaseType::ToJsonString(bool pretty) const {
  nlohmann::json j = ToJson();
  if (pretty) {
    return j.dump(JSON_TAB_SIZE);
  }
  return j.dump();
}

int32_t ArrayType::MaxBytes() const {
  int32_t total_size = 0;
  
  // 整数数组
  total_size += sizeof(int32_t);  // 数组长度的定长编码(4字节)
  total_size += array_i32.size() * 5;  // 每个整数使用变长编码，最多需要5字节
  
  // 字符串数组
  total_size += sizeof(int32_t);  // 数组长度的定长编码(4字节)
  // 每个字符串: 长度的变长编码(最多5字节) + 字符串内容
  for (const auto& str : array_str) {
    total_size += 5 + str.length();
  }
  
  return total_size;
}

int32_t ArrayType::MinBytes() const {
  // 两个空数组的长度编码，每个4字节
  return sizeof(int32_t) * 2;
}

int32_t ArrayType::ToString(std::string& output) const {
  // 预分配足够的空间，避免多次内存分配
  output.resize(MaxBytes());
  
  // 直接调用ToBytes，因为std::string内部是连续的字符数组
  int32_t bytes = ToBytes(&output[0], output.size());
  if (bytes < 0) {
    return -1;
  }
  
  // 调整字符串大小为实际使用的长度
  output.resize(bytes);
  return bytes;
}

int32_t ArrayType::FromString(const std::string& input) {
  if (input.empty()) {
    return -1;
  }
  
  // 直接调用FromBytes，因为std::string内部是连续的字符数组
  return FromBytes(input.data(), input.length());
}

int32_t ArrayType::ToBytes(char* output, int32_t len) const {
  if (output == nullptr || len <= 0) {
    return -1;
  }
  
  // 检查输出缓冲区长度是否足够
  if (len < MaxBytes()) {
    return -1;
  }
  
  int32_t total_bytes = 0;
  
  // 序列化整数数组
  // 写入数组长度
  EncodeFixed32(output + total_bytes, array_i32.size());
  total_bytes += sizeof(int32_t);
  
  // 写入数组元素
  for (int32_t value : array_i32) {
    char* end = EncodeVarint32(output + total_bytes, value);
    if (end == nullptr) {
      return -1;
    }
    total_bytes += end - (output + total_bytes);
  }
  
  // 序列化字符串数组
  // 写入数组长度
  EncodeFixed32(output + total_bytes, array_str.size());
  total_bytes += sizeof(int32_t);
  
  // 写入数组元素
  for (const auto& str : array_str) {
    char* end = EncodeString2(output + total_bytes, len - total_bytes, Slice(str));
    if (end == nullptr) {
      return -1;
    }
    total_bytes += end - (output + total_bytes);
  }
  
  return total_bytes;
}

int32_t ArrayType::FromBytes(const char* input, int32_t len) {
  if (input == nullptr || len <= 0) {
    return -1;
  }
  
  // 检查输入长度是否足够
  if (len < MinBytes()) {
    return -1;
  }
  
  int32_t total_bytes = 0;
  
  // 解析整数数组
  // 读取数组长度
  uint32_t i32_size = DecodeFixed32(input + total_bytes);
  total_bytes += sizeof(int32_t);
  
  // 清空并预分配空间
  array_i32.clear();
  array_i32.reserve(i32_size);
  
  // 读取数组元素
  for (uint32_t i = 0; i < i32_size; i++) {
    Slice remaining(input + total_bytes, len - total_bytes);
    uint32_t value;
    if (!DecodeVarint32(&remaining, &value)) {
      return -1;
    }
    array_i32.push_back(value);
    total_bytes += remaining.data() - (input + total_bytes);
  }
  
  // 解析字符串数组
  // 读取数组长度
  uint32_t str_size = DecodeFixed32(input + total_bytes);
  total_bytes += sizeof(int32_t);
  
  // 清空并预分配空间
  array_str.clear();
  array_str.reserve(str_size);
  
  // 读取数组元素
  for (uint32_t i = 0; i < str_size; i++) {
    Slice remaining(input + total_bytes, len - total_bytes);
    Slice str;
    int32_t bytes = DecodeString2(&remaining, &str);
    if (bytes < 0) {
      return -1;
    }
    array_str.push_back(str.ToString());
    total_bytes += bytes;
  }
  
  return total_bytes;
}

nlohmann::json ArrayType::ToJson() const {
  nlohmann::json j;
  
  // 序列化整数数组
  j["array_i32"] = array_i32;
  
  // 序列化字符串数组
  j["array_str"] = array_str;
  
  return j;
}

const std::string ArrayType::ToJsonString(bool pretty) const {
  nlohmann::json j = ToJson();
  if (pretty) {
    return j.dump(JSON_TAB_SIZE);
  }
  return j.dump();
}

}  // namespace vgraph
