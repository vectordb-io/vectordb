#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cassert>
#include <set>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include "meta.h"
#include "util.h"

namespace vectordb {

namespace util {

void
Split(const std::string &s, char separator, std::vector<std::string> &sv, const std::string ignore) {
    std::set<char> ignore_chars;
    for (auto c : ignore) {
        ignore_chars.insert(c);
    }

    std::string sub_str;
    for (auto c : s) {
        auto it = ignore_chars.find(c);
        if (it != ignore_chars.end()) {
            continue;
        }

        if (c != separator) {
            sub_str.push_back(c);
        } else {
            if (!sub_str.empty()) {
                sv.push_back(sub_str);
            }
            sub_str.clear();
        }
    }

    if (!sub_str.empty()) {
        sv.push_back(sub_str);
    }
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
ToLower(std::string &str) {
    for (size_t i = 0; i < str.size(); i++)
        str[i] = tolower(str[i]);
}

std::string
PartitionName(const std::string &table_name, int partition_id) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s#partition%d", table_name.c_str(), partition_id);
    return std::string(buf);
}

std::string
ReplicaName(const std::string &table_name, int partition_id, int replica_id) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s#partition%d#replica%d", table_name.c_str(), partition_id, replica_id);
    return std::string(buf);
}

bool
ParsePartitionName(const std::string &partition_name, std::string &table_name, int &partition_id) {
    std::vector<std::string> sv;
    Split(partition_name, '#', sv);
    if (sv.size() != 2) {
        return false;
    }
    table_name = sv[0];
    sscanf(sv[1].c_str(), "%d", &partition_id);
    return true;
}

bool
ParseReplicaName(const std::string &replica_name, std::string &table_name, int &partition_id, int &replica_id) {
    std::vector<std::string> sv;
    Split(replica_name, '#', sv);
    if (sv.size() != 3) {
        return false;
    }
    table_name = sv[0];
    sscanf(sv[1].c_str(), "%d", &partition_id);
    sscanf(sv[2].c_str(), "%d", &replica_id);
    return true;
}

} // namespace util

}  // namespace vectordb
