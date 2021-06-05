#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cassert>
#include "meta.h"
#include "util.h"

namespace vectordb {

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


}  // namespace vectordb
