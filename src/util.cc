#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cassert>
#include <string>
#include <sstream>
#include <iostream>
#include "meta.h"
#include "util.h"

namespace vectordb {

void ToLower(std::string &str) {
    for (int i=0; i <str.size(); i++)
        str[i] = tolower(str[i]);
}

bool
DirOK(const std::string &path) {
    DIR* dir = opendir(path.c_str());
    if (dir != nullptr) {
        closedir(dir);
        return true;
    } else {
        return false;
    }
}

void
Mkdir(const std::string &path) {
    int ret = mkdir(path.c_str(), 0775);
    assert(ret == 0);
}

void
Parse(std::string &line, std::vector<std::string> &argv) {
    std::string s;
    for (auto &ch : line) {
        if (ch != ' ' && ch != '\t') {
            std::stringstream stream;
            stream << ch;
            s.append(stream.str());
        } else {
            if (s.length() > 0) {
                argv.push_back(s);
            }
            s.clear();
        }
    }
    if (s.length() > 0) {
        argv.push_back(s);
    }
}

}  // namespace vectordb
