#ifndef VGRAPH_UTIL_CODING_H_
#define VGRAPH_UTIL_CODING_H_

#include <cstdint>
#include <cstring>

#include "slice.h"

namespace vgraph {

// 初始化相关函数
/**
 * @brief 检测当前系统是否为小端字节序
 * @return true 表示系统为小端字节序，false 表示系统为大端字节序
 *
 * 通过将一个多字节整数存入内存并检查第一个字节来判断系统字节序:
 * - 如果第一个字节是最低有效字节，则为小端字节序
 * - 如果第一个字节是最高有效字节，则为大端字节序
 */
bool IsLittleEndian();

/**
 * @brief 初始化编码模块
 *
 * 调用 IsLittleEndian() 检测系统字节序并设置全局字节序标志，
 * 这个标志会影响后续所有的编解码操作
 */
void CodingInit();

// 调试打印相关函数
/**
 * @brief 将一个字节转换为二进制字符串表示
 * @param byte 要转换的字节
 * @return 8位二进制字符串，例如"10101010"
 *
 * 从高位到低位依次检查每一位:
 * - 如果该位为1，在结果字符串中添加'1'
 * - 如果该位为0，在结果字符串中添加'0'
 */
std::string ByteToBitString(char byte);

/**
 * @brief 从低位到高位打印字节序列的二进制表示
 * @param ptr 字节序列的起始地址
 * @param len 字节序列的长度
 * @return 二进制字符串表示
 *
 * 从低地址到高地址遍历字节序列:
 * - 对每个字节调用 ByteToBitString 获取其二进制表示
 * - 将所有字节的二进制表示连接成一个字符串
 */
std::string BitsStringFromLow(char *ptr, int32_t len);

/**
 * @brief 从高位到低位打印字节序列的二进制表示
 * @param ptr 字节序列的起始地址
 * @param len 字节序列的长度
 * @return 二进制字符串表示
 *
 * 从高地址到低地址遍历字节序列:
 * - 对每个字节调用 ByteToBitString 获取其二进制表示
 * - 将所有字节的二进制表示连接成一个字符串
 */
std::string BitsStringFromHigh(char *ptr, int32_t len);

/**
 * @brief 打印字节序列的二进制和十六进制表示
 * @param ptr 字节序列的起始地址
 * @param len 字节序列的长度
 * @param from_low true表示从低位到高位打印，false表示从高位到低位打印
 *
 * 打印格式包括:
 * - 地址范围信息
 * - 按8位分组的二进制表示
 * - 每个字节的十六进制表示
 */
void PrintBin(char *ptr, int32_t len, bool from_low);

// 8位整数编解码函数
/**
 * @brief 将8位无符号整数编码到目标内存
 * @param dst 目标内存地址
 * @param value 要编码的8位无符号整数
 *
 * 直接将8位整数复制到目标内存，不考虑字节序问题
 */
void EncodeFixed8(char *dst, uint8_t value);

/**
 * @brief 从内存中解码8位无符号整数
 * @param ptr 源内存地址
 * @return 解码得到的8位无符号整数
 *
 * 直接从内存读取8位整数，不考虑字节序问题
 */
uint8_t DecodeFixed8(const char *ptr);

/**
 * @brief 将8位无符号整数编码为变长格式
 * @param dst 目标内存地址
 * @param value 要编码的8位无符号整数
 * @return 编码后的结束位置
 *
 * 使用变长编码格式，小的数字使用更少的字节
 */
char *EncodeVarint8(char *dst, uint8_t value);

/**
 * @brief 从变长格式解码8位无符号整数
 * @param input 输入数据切片，解码后会更新位置
 * @param value 解码结果的存储位置
 * @return 解码是否成功
 */
bool DecodeVarint8(Slice *input, uint8_t *value);

// 16位整数编解码函数
/**
 * @brief 将16位无符号整数编码到目标内存
 * @param dst 目标内存地址
 * @param value 要编码的16位无符号整数
 *
 * 根据系统字节序:
 * - 小端系统直接复制
 * - 大端系统需要交换字节顺序
 */
void EncodeFixed16(char *dst, uint16_t value);

/**
 * @brief 从内存中解码16位无符号整数
 * @param ptr 源内存地址
 * @return 解码得到的16位无符号整数
 *
 * 根据系统字节序:
 * - 小端系统直接读取
 * - 大端系统需要交换字节顺序
 */
uint16_t DecodeFixed16(const char *ptr);

// 32位整数编解码函数
/**
 * @brief 将32位无符号整数编码到目标内存
 * @param dst 目标内存地址
 * @param value 要编码的32位无符号整数
 *
 * 根据系统字节序:
 * - 小端系统直接复制
 * - 大端系统需要交换字节顺序
 */
void EncodeFixed32(char *dst, uint32_t value);

/**
 * @brief 从内存中解码32位无符号整数
 * @param ptr 源内存地址
 * @return 解码得到的32位无符号整数
 *
 * 根据系统字节序:
 * - 小端系统直接读取
 * - 大端系统需要交换字节顺序
 */
uint32_t DecodeFixed32(const char *ptr);

/**
 * @brief 将32位无符号整数编码为变长格式
 * @param dst 目标内存地址
 * @param value 要编码的32位无符号整数
 * @return 编码后的结束位置
 *
 * 使用变长编码格式，小的数字使用更少的字节
 */
char *EncodeVarint32(char *dst, uint32_t value);

/**
 * @brief 从变长格式解码32位无符号整数
 * @param input 输入数据切片，解码后会更新位置
 * @param value 解码结果的存储位置
 * @return 解码是否成功
 */
bool DecodeVarint32(Slice *input, uint32_t *value);

/**
 * @brief 从变长格式解码32位无符号整数
 * @param input 输入数据切片，解码后会更新位置
 * @param value 解码结果的存储位置
 * @return 解码得到的字节数
 *
 * 使用变长编码格式，小的数字使用更少的字节
 */
int32_t DecodeVarint32Bytes(Slice *input, uint32_t *value);

// 64位整数编解码函数
/**
 * @brief 将64位无符号整数编码到目标内存
 * @param dst 目标内存地址
 * @param value 要编码的64位无符号整数
 *
 * 根据系统字节序:
 * - 小端系统直接复制
 * - 大端系统需要交换字节顺序
 */
void EncodeFixed64(char *dst, uint64_t value);

/**
 * @brief 从内存中解码64位无符号整数
 * @param ptr 源内存地址
 * @return 解码得到的64位无符号整数
 *
 * 根据系统字节序:
 * - 小端系统直接读取
 * - 大端系统需要交换字节顺序
 */
uint64_t DecodeFixed64(const char *ptr);

/**
 * @brief 将64位无符号整数编码为变长格式
 * @param dst 目标内存地址
 * @param value 要编码的64位无符号整数
 * @return 编码后的结束位置
 *
 * 使用变长编码格式，小的数字使用更少的字节
 */
char *EncodeVarint64(char *dst, uint64_t value);

/**
 * @brief 从变长格式解码64位无符号整数
 * @param input 输入数据切片，解码后会更新位置
 * @param value 解码结果的存储位置
 * @return 解码是否成功
 */
bool DecodeVarint64(Slice *input, uint64_t *value);

// float编解码函数
/**
 * @brief 将float编码到目标内存
 * @param dst 目标内存地址
 * @param value 要编码的float
 *
 * 根据系统字节序:
 * - 小端系统直接复制
 * - 大端系统需要交换字节顺序
 */
void EncodeFloat(char *dst, float value);

/**
 * @brief 从内存中解码float
 * @param ptr 源内存地址
 * @return 解码得到的float
 *
 * 根据系统字节序:
 * - 小端系统直接读取
 * - 大端系统需要交换字节顺序
 */
float DecodeFloat(const char *ptr);

/**
 * @brief 将double编码到目标内存
 * @param dst 目标内存地址
 * @param value 要编码的double
 *
 * 根据系统字节序:
 * - 小端系统直接复制
 * - 大端系统需要交换字节顺序
 */
void EncodeDouble(char *dst, double value);

/**
 * @brief 从内存中解码double
 * @param ptr 源内存地址
 * @return 解码得到的double
 *
 * 根据系统字节序:
 * - 小端系统直接读取
 * - 大端系统需要交换字节顺序
 */
double DecodeDouble(const char *ptr);

// string编解码函数
/**
 * @brief 将string编码到目标内存
 * @param dst 目标内存地址
 * @param src 要编码的string, 使用Slice表示
 *
 * 根据系统字节序:
 * - ��端系统直接复制
 * - 大端系统需要交换字节顺序
 */
void EncodeString(std::string *dst, const Slice &src);

/**
 * @brief 从内存中解码string
 * @param input 输入数据切片，解码后会更新位置
 * @param result 解码结果的存储位置
 * @return 解码是否成功
 */
bool DecodeString(Slice *input, Slice *result);

/**
 * @brief 将string编码为变长格式
 * @param dst 目标内存地址
 * @param len 目标缓冲区的大小
 * @param src 要编码的string, 使用Slice表示
 * @return 编码成功时返回编码后的结束位置，缓冲区大小不足时返回nullptr
 *
 * 使用变长编码格式存储字符串长度，然后存储字符串内容:
 * - 检查目标缓冲区大小是否足够
 * - 如果缓冲区太小返回nullptr
 * - 字符串长度使用变长编码，小的数字使用更少的字节
 */
char *EncodeString2(const char *dst, int32_t len, const Slice &src);

/**
 * @brief 从变长格式解码string
 * @param input 输入数据切片，解码后会更新位置
 * @param result 解码结果的存储位置
 * @return 解码得到的字节数
 *
 * 使用变长编码格式，小的数字使用更少的字节
 */
int32_t DecodeString2(Slice *input, Slice *result);

}  // namespace vgraph

#endif  // VGRAPH_UTIL_CODING_H_
