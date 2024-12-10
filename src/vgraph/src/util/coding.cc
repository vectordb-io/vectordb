#include "coding.h"

namespace vgraph {

// 静态辅助函数
// --------------------------------------------------------------------

/**
 * @brief 从内存中解码32位变长整数的回退实现
 * 
 * @param p 输入数据的起始位置
 * @param limit 输入数据的结束位置
 * @param value 解码后的值存储位置
 * @return const char* 解码后的数据位置，解码失败返回nullptr
 */
static const char *GetVarint32PtrFallback(const char *p, const char *limit,
                                          uint32_t *value) {
  uint32_t result = 0;
  for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
    uint32_t byte = *(reinterpret_cast<const unsigned char *>(p));
    p++;
    if (byte & 128) {
      // More bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char *>(p);
    }
  }
  return nullptr;
}

/**
 * @brief 从内存中解码32位变长整数
 * 
 * @param p 输入数据的起始位置
 * @param limit 输入数据的结束位置
 * @param value 解码后的值存储位置
 * @return const char* 解码后的数据位置，解码失败返回nullptr
 */
static const char *GetVarint32Ptr(const char *p, const char *limit,
                                  uint32_t *value) {
  if (p < limit) {
    uint32_t result = *(reinterpret_cast<const unsigned char *>(p));
    if ((result & 128) == 0) {
      *value = result;
      return p + 1;
    }
  }
  return GetVarint32PtrFallback(p, limit, value);
}

/**
 * @brief 从内存中解码64位变长整数
 * 
 * @param p 输入数据的起始位置
 * @param limit 输入数据的结束位置
 * @param value 解码后的值存储位置
 * @return const char* 解码后的数据位置，解码失败返回nullptr
 */
static const char *GetVarint64Ptr(const char *p, const char *limit,
                                  uint64_t *value) {
  uint64_t result = 0;
  for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
    uint64_t byte = *(reinterpret_cast<const unsigned char *>(p));
    p++;
    if (byte & 128) {
      // More bytes are present
      result |= ((byte & 127) << shift);
    } else {
      result |= (byte << shift);
      *value = result;
      return reinterpret_cast<const char *>(p);
    }
  }
  return nullptr;
}

/**
 * @brief 将32位变长整数编码到字符串中
 * 
 * @param dst 目标字符串
 * @param v 要编码的值
 */
static void PutVarint32(std::string *dst, uint32_t v) {
  char buf[5];
  char *ptr = EncodeVarint32(buf, v);
  dst->append(buf, ptr - buf);
}

// 全局变量
static bool g_little_endian = true;  // 系统字节序标志

/**
 * @brief 检测系统是否为小端字节序
 * 
 * @return true 系统为小端字节序
 * @return false 系统为大端字节序
 */
bool IsLittleEndian() {
  union {
    uint32_t value;
    uint8_t bytes[4];
  } test;

  test.value = 0x01;

  if (test.bytes[0] == 0x01) {
    return true;
  } else {
    return false;
  }
}

/**
 * @brief 初始化编码模块，设置全局字节序标志
 */
void CodingInit() { g_little_endian = IsLittleEndian(); }

std::string ByteToBitString(char byte) {
  std::string s;
  for (int32_t i = 7; i >= 0; i--) {
    char c = (byte & (1 << i)) ? '1' : '0';
    s.push_back(c);
  }
  return s;
}

std::string BitsStringFromLow(char *ptr, int32_t len) {
  assert(len > 0);
  std::string s;
  for (int32_t i = 0; i < len; ++i) {
    std::string s_char = ByteToBitString(ptr[i]);
    s.append(s_char);
  }
  return s;
}

std::string BitsStringFromHigh(char *ptr, int32_t len) {
  assert(len > 0);
  std::string s;
  for (int32_t i = len - 1; i >= 0; --i) {
    std::string s_char = ByteToBitString(ptr[i]);
    s.append(s_char);
  }
  return s;
}

void PrintBin(char *ptr, int32_t len, bool from_low) {
  assert(len > 0);

  std::string s;
  char addr[128];
  if (from_low) {
    s = BitsStringFromLow(ptr, len);
    snprintf(addr, sizeof(addr), "low - high, %p - %p:", ptr, ptr + (len - 1));
  } else {
    s = BitsStringFromHigh(ptr, len);
    snprintf(addr, sizeof(addr), "high - low, %p - %p:", ptr + (len - 1), ptr);
  }

  printf("%s", addr);
  for (uint32_t i = 0; i < s.length(); ++i) {
    if (i % 8 == 0) {
      printf(" ");
    }
    printf("%c", s[i]);
  }
  printf("\n");

  printf("%s", addr);
  if (from_low) {
    for (int32_t i = 0; i < len; ++i) {
      unsigned char c = ptr[i];
      printf(" 0x%02x", c);
    }
  } else {
    for (int32_t i = len - 1; i >= 0; --i) {
      unsigned char c = ptr[i];
      printf(" 0x%02x", c);
    }
  }
  printf("\n");
}

void EncodeFixed8(char *dst, uint8_t value) {
  memcpy(dst, &value, sizeof(value));
}

uint8_t DecodeFixed8(const char *ptr) {
  uint8_t result;
  memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
  return result;
}

char *EncodeVarint8(char *dst, uint8_t value) {
  // 如果value小于128,直接编码为一个字节
  if (value < (1 << 7)) {
    *dst = value;
    return dst + 1;
  }
  
  // 否则需要两个字节编码
  // 第一个字节:最高位置1,低7位存储value的低7位
  dst[0] = value | 0x80;
  // 第二个字节:存储value的高1位
  dst[1] = value >> 7;
  return dst + 2;
}

bool DecodeVarint8(Slice *input, uint8_t *value) {
  if (input->empty()) {
    return false;
  }

  // 读取第一个字节
  uint8_t byte = static_cast<uint8_t>(input->data()[0]);
  input->remove_prefix(1);

  // 如果最高位为0,说明只有一个字节
  if ((byte & 0x80) == 0) {
    *value = byte;
    return true;
  }

  // 否则需要读取第二个字节
  if (input->empty()) {
    return false;
  }

  // 第一个字节的低7位
  *value = byte & 0x7f;
  
  // 读取第二个字节,作为高位
  byte = static_cast<uint8_t>(input->data()[0]);
  input->remove_prefix(1);
  
  // 将第二个字节左移7位,与第一个字节的值组合
  *value |= (byte << 7);
  
  return true;
}

void EncodeFixed16(char *dst, uint16_t value) {
  if (g_little_endian) {
    memcpy(dst, &value, sizeof(value));
  } else {
    dst[0] = value & 0xff;
    dst[1] = (value >> 8) & 0xff;
  }
}

uint16_t DecodeFixed16(const char *ptr) {
  if (g_little_endian) {
    // Load the raw bytes
    uint16_t result;
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    return ((static_cast<uint32_t>(static_cast<unsigned char>(ptr[0]))) |
            (static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8));
  }
}

char *EncodeVarint16(char *dst, uint16_t value) {
  // 如果value小于128,直接编码为一个字节
  if (value < (1 << 7)) {
    *dst = value;
    return dst + 1;
  }
  
  // 如果value小于16384(2^14),用两个字节编码
  if (value < (1 << 14)) {
    dst[0] = value | 0x80;  // 最高位置1,低7位存储value的低7位
    dst[1] = (value >> 7);  // 存储value的高7位
    return dst + 2;
  }
  
  // 否则需要三个字节编码
  dst[0] = value | 0x80;  // 最高位置1,低7位存储value的低7位
  dst[1] = ((value >> 7) | 0x80);  // 最高位置1,低7位存储value的中间7位
  dst[2] = value >> 14;  // 存储value的高2位
  return dst + 3;
}

bool DecodeVarint16(Slice *input, uint16_t *value) {
  if (input->empty()) {
    return false;
  }

  // 读取第一个字节
  uint16_t result = 0;
  uint8_t byte = static_cast<uint8_t>(input->data()[0]);
  input->remove_prefix(1);

  // 如果最高位为0,说明只有一个字节
  if ((byte & 0x80) == 0) {
    *value = byte;
    return true;
  }

  // 第一个字节的低7位
  result = byte & 0x7f;

  // 如果没有更多字节,返回false
  if (input->empty()) {
    return false;
  }

  // 读取第二个字节
  byte = static_cast<uint8_t>(input->data()[0]);
  input->remove_prefix(1);

  // 如果最高位为0,说明只有两个字节
  if ((byte & 0x80) == 0) {
    result |= (static_cast<uint16_t>(byte) << 7);
    *value = result;
    return true;
  }

  // 第二个字节的低7位
  result |= ((byte & 0x7f) << 7);

  // 如果没有更多字节,返回false
  if (input->empty()) {
    return false;
  }

  // 读取第三个字节
  byte = static_cast<uint8_t>(input->data()[0]);
  input->remove_prefix(1);

  // 第三个字节最多2位
  result |= (static_cast<uint16_t>(byte) << 14);
  *value = result;
  return true;
}

void EncodeFixed32(char *dst, uint32_t value) {
  if (g_little_endian) {
    memcpy(dst, &value, sizeof(value));
  } else {
    dst[0] = value & 0xff;
    dst[1] = (value >> 8) & 0xff;
    dst[2] = (value >> 16) & 0xff;
    dst[3] = (value >> 24) & 0xff;
  }
}

uint32_t DecodeFixed32(const char *ptr) {
  if (g_little_endian) {
    // Load the raw bytes
    uint32_t result;
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    return ((static_cast<uint32_t>(static_cast<unsigned char>(ptr[0]))) |
            (static_cast<uint32_t>(static_cast<unsigned char>(ptr[1])) << 8) |
            (static_cast<uint32_t>(static_cast<unsigned char>(ptr[2])) << 16) |
            (static_cast<uint32_t>(static_cast<unsigned char>(ptr[3])) << 24));
  }
}

char *EncodeVarint32(char *dst, uint32_t v) {
  // Operate on characters as unsigneds
  unsigned char *ptr = reinterpret_cast<unsigned char *>(dst);
  static const int B = 128;
  if (v < (1 << 7)) {
    *(ptr++) = v;
  } else if (v < (1 << 14)) {
    *(ptr++) = v | B;
    *(ptr++) = v >> 7;
  } else if (v < (1 << 21)) {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = v >> 14;
  } else if (v < (1 << 28)) {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = v >> 21;
  } else {
    *(ptr++) = v | B;
    *(ptr++) = (v >> 7) | B;
    *(ptr++) = (v >> 14) | B;
    *(ptr++) = (v >> 21) | B;
    *(ptr++) = v >> 28;
  }
  return reinterpret_cast<char *>(ptr);
}

bool DecodeVarint32(Slice *input, uint32_t *value) {
  const char *p = input->data();
  const char *limit = p + input->size();
  const char *q = GetVarint32Ptr(p, limit, value);
  if (q == nullptr) {
    return false;
  } else {
    *input = Slice(q, limit - q);
    return true;
  }
}

// return real decode bytes
int32_t DecodeVarint32Bytes(Slice *input, uint32_t *value) {
  const char *p = input->data();
  const char *limit = p + input->size();
  const char *q = GetVarint32Ptr(p, limit, value);
  if (q == nullptr) {
    return -1;
  } else {
    *input = Slice(q, limit - q);
    return (q - p);
  }
}

void EncodeFloat(char *dst, float value) {
  uint32_t u32 = *((uint32_t *)&value);
  EncodeFixed32(dst, u32);
}

float DecodeFloat(const char *ptr) {
  uint32_t u32 = DecodeFixed32(ptr);
  float f = *((float *)&u32);
  return f;
}

void EncodeFixed64(char *dst, uint64_t value) {
  if (g_little_endian) {
    memcpy(dst, &value, sizeof(value));
  } else {
    dst[0] = value & 0xff;
    dst[1] = (value >> 8) & 0xff;
    dst[2] = (value >> 16) & 0xff;
    dst[3] = (value >> 24) & 0xff;
    dst[4] = (value >> 32) & 0xff;
    dst[5] = (value >> 40) & 0xff;
    dst[6] = (value >> 48) & 0xff;
    dst[7] = (value >> 56) & 0xff;
  }
}

uint64_t DecodeFixed64(const char *ptr) {
  if (g_little_endian) {
    // Load the raw bytes
    uint64_t result;
    memcpy(&result, ptr, sizeof(result));  // gcc optimizes this to a plain load
    return result;
  } else {
    uint64_t lo = DecodeFixed32(ptr);
    uint64_t hi = DecodeFixed32(ptr + 4);
    return (hi << 32) | lo;
  }
}

char *EncodeVarint64(char *dst, uint64_t v) {
  static const int B = 128;
  unsigned char *ptr = reinterpret_cast<unsigned char *>(dst);
  while (v >= B) {
    *(ptr++) = v | B;
    v >>= 7;
  }
  *(ptr++) = static_cast<unsigned char>(v);
  return reinterpret_cast<char *>(ptr);
}

bool DecodeVarint64(Slice *input, uint64_t *value) {
  const char *p = input->data();
  const char *limit = p + input->size();
  const char *q = GetVarint64Ptr(p, limit, value);
  if (q == nullptr) {
    return false;
  } else {
    *input = Slice(q, limit - q);
    return true;
  }
}

void EncodeDouble(char *dst, double value) {
  uint64_t u64 = *((uint64_t *)&value);
  EncodeFixed64(dst, u64);
}

double DecodeDouble(const char *ptr) {
  uint64_t u64 = DecodeFixed64(ptr);
  double d = *((double *)&u64);
  return d;
}

void EncodeString(std::string *dst, const Slice &value) {
  PutVarint32(dst, value.size());
  dst->append(value.data(), value.size());
}

bool DecodeString(Slice *input, Slice *result) {
  uint32_t len;
  if (DecodeVarint32(input, &len) && input->size() >= len) {
    *result = Slice(input->data(), len);
    input->remove_prefix(len);
    return true;
  } else {
    return false;
  }
}

char *EncodeString2(const char *dst, int32_t len, const Slice &value) {
  // 检查目标缓冲区大小是否足够
  // 需要的空间 = 变长编码的长度(最多5字节) + 字符串内容的长度
  if (len < 5 + static_cast<int32_t>(value.size())) {
    return nullptr;  // 缓冲区太小
  }

  char *p = const_cast<char *>(dst);
  char *p2 = EncodeVarint32(p, value.size());
  p = p2;

  memcpy(p, value.data(), value.size());
  p += value.size();

  return p;
}

// return real decode bytes
int32_t DecodeString2(Slice *input, Slice *result) {
  uint32_t len;
  int32_t bytes = DecodeVarint32(input, &len);
  if (bytes > 0 && input->size() >= len) {
    *result = Slice(input->data(), len);
    input->remove_prefix(len);
    return bytes + len;
  } else {
    return -1;
  }
}

}  // namespace vgraph
