#ifndef __VECTORDB_UTIL_H__
#define __VECTORDB_UTIL_H__

#include <vector>
#include <string>

namespace vectordb {

namespace util {

void ToLower(std::string &str);
bool DirOK(const std::string &path);
void Mkdir(const std::string &path);

} // namespace util

}  // namespace vectordb

#endif
