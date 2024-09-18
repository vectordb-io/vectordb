#ifndef VRAFT_UTIL_H_
#define VRAFT_UTIL_H_

#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace vraft {

void Split(const std::string &str, char separator,
           std::vector<std::string> &result);

void DelSpace(std::string &str);

std::string CurrentTidStr();

void ToLower(std::string &input);

// ip_out是主机字节序，小端
bool HostNameToIpU32(const std::string &hostname, uint32_t &ip_out);

// ip_out是主机字节序，小端
bool IpStringToIpU32(const std::string &ip_str, uint32_t &ip_out);

// ip_out是主机字节序，小端
bool StringToIpU32(const std::string &str, uint32_t &ip_out);

// ip是主机字节序，小端
std::string IpU32ToIpString(uint32_t ip);

uint32_t Crc32(const void *data, size_t length);

std::string StrToHexStr(const char *ptr, int32_t size);

std::string PointerToHexStr(void *p);

std::string U32ToHexStr(uint32_t x);

std::string NsToString(uint64_t ns);

std::string NsToString2(uint64_t ns);

bool IsNumber(const std::string &str);

std::string TidToStr(std::thread::id);

std::string CurrentTid();

bool TidValid(std::thread::id);

uint32_t UniqId(void *ptr);

void ConvertStringToArgcArgv(const std::string &s, int *argc, char ***argv);
void FreeArgv(int argc, char **argv);

float RandomFloat(float max);

bool IsDirExist(const std::string &dir_path);

bool IsFileExist(const std::string &path);

int32_t PartitionId(const std::string &key, int32_t partition_num);

void DelHead(std::string &s, const std::string &del);

void DelTail(std::string &s, const std::string &del);

void ListDir(const std::string &path, std::vector<std::string> &paths);

int32_t AtomicMove(const std::string &src_path, const std::string &dest_path);

}  // namespace vraft

#endif
