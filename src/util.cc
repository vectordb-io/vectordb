#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cassert>
#include <set>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <glog/logging.h>
#include "meta.h"
#include "util.h"

namespace vectordb {

namespace util {

void
Split(const std::string &s, char separator, std::vector<std::string> &sv, const std::string ignore) {
    sv.clear();

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
    snprintf(buf, sizeof(buf), "%s#partition_%d", table_name.c_str(), partition_id);
    return std::string(buf);
}

std::string
ReplicaName(const std::string &table_name, int partition_id, int replica_id) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s#partition_%d#replica_%d", table_name.c_str(), partition_id, replica_id);
    return std::string(buf);
}

bool
ParsePartitionName(const std::string &partition_name, std::string &table_name, int &partition_id) {
    std::vector<std::string> sv, sv_id;
    Split(partition_name, '#', sv);
    if (sv.size() != 2) {
        return false;
    }
    table_name = sv[0];

    Split(sv[1], '_', sv_id);
    if (sv_id.size() != 2) {
        return false;
    }
    sscanf(sv_id[1].c_str(), "%d", &partition_id);
    return true;
}

bool
ParseReplicaName(const std::string &replica_name, std::string &table_name, int &partition_id, int &replica_id) {
    std::vector<std::string> sv, sv_id;
    Split(replica_name, '#', sv);
    if (sv.size() != 3) {
        return false;
    }
    table_name = sv[0];

    Split(sv[1], '_', sv_id);
    if (sv_id.size() != 2) {
        return false;
    }
    sscanf(sv_id[1].c_str(), "%d", &partition_id);

    Split(sv[2], '_', sv_id);
    if (sv_id.size() != 2) {
        return false;
    }
    sscanf(sv_id[1].c_str(), "%d", &replica_id);

    LOG(INFO) << "debug: " << sv[1].c_str() << " " << sv[2].c_str() << " " << partition_id << " " << replica_id;

    return true;
}

} // namespace util

} // namespace vectordb
