#ifndef VRAFT_CODING_H_
#define VRAFT_CODING_H_

#include <cstdint>
#include <cstring>

#include "slice.h"

namespace vraft {

// init
bool IsLittleEndian();
void CodingInit();

// debug print
std::string ByteToBitString(char byte);
std::string BitsStringFromLow(char *ptr, int32_t len);
std::string BitsStringFromHigh(char *ptr, int32_t len);
void PrintBin(char *ptr, int32_t len, bool from_low);

// 8
void EncodeFixed8(char *dst, uint8_t value);
uint8_t DecodeFixed8(const char *ptr);

char *EncodeVarint8(char *dst, uint8_t value);
bool DecodeVarint8(Slice *input, uint8_t *value);

// 16
void EncodeFixed16(char *dst, uint16_t value);
uint16_t DecodeFixed16(const char *ptr);

char *EncodeVarint16(char *dst, uint16_t value);
bool DecodeVarint16(Slice *input, uint16_t *value);

// 32
void EncodeFixed32(char *dst, uint32_t value);
uint32_t DecodeFixed32(const char *ptr);

char *EncodeVarint32(char *dst, uint32_t value);
bool DecodeVarint32(Slice *input, uint32_t *value);
int32_t DecodeVarint32Bytes(Slice *input, uint32_t *value);

// 64
void EncodeFixed64(char *dst, uint64_t value);
uint64_t DecodeFixed64(const char *ptr);

char *EncodeVarint64(char *dst, uint64_t value);
bool DecodeVarint64(Slice *input, uint64_t *value);

// float
void EncodeFloat(char *dst, float value);
float DecodeFloat(const char *ptr);

void EncodeDouble(char *dst, double value);
double DecodeDouble(const char *ptr);

// string
void EncodeString(std::string *dst, const Slice &value);
bool DecodeString(Slice *input, Slice *result);

char *EncodeString2(const char *dst, int32_t len, const Slice &value);
int32_t DecodeString2(Slice *input, Slice *result);

}  // namespace vraft

#endif
