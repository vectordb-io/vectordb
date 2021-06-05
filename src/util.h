#ifndef __VECTORDB_UTIL_H__
#define __VECTORDB_UTIL_H__

#include <string>

namespace vectordb {

bool DirOK(const std::string &path);
void Mkdir(const std::string &path);


}  // namespace vectordb

#endif
