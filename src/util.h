#ifndef __VECTORDB_UTIL_H__
#define __VECTORDB_UTIL_H__

#include <vector>
#include <string>

namespace vectordb {

bool DirOK(const std::string &path);
void Mkdir(const std::string &path);

void ToLower(std::string &str);
void Parse(std::string &line, std::vector<std::string> &argv);

}  // namespace vectordb

#endif
