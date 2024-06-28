#include "coding.h"

namespace vraft {

// static function
// --------------------------------------------------------------------
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

static void PutVarint32(std::string *dst, uint32_t v) {
  char buf[5];
  char *ptr = EncodeVarint32(buf, v);
  dst->append(buf, ptr - buf);
}

// ------------------------------------------------------------------------------------

static bool g_little_endian = true;

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
  assert(0);
  return nullptr;
}

bool DecodeVarint8(Slice *input, uint8_t *value) {
  assert(0);
  return false;
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
  assert(0);
  return nullptr;
}

bool DecodeVarint16(Slice *input, uint16_t *value) {
  assert(0);
  return false;
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

}  // namespace vraft
