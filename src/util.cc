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

bool
RemoveDir(const std::string &path) {
    std::string cmd = "rm -rf ";
    cmd.append(path);
    int ret = system(cmd.c_str());
    return ret == 0;
}

bool
ChildrenOfDir(const std::string &path, std::vector<std::string> &children_paths, std::vector<std::string> &children_names) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return false;
    }

    children_paths.clear();
    children_names.clear();
    dirent *direntp;
    while ((direntp = readdir(dir)) != nullptr) {
        if (std::string(direntp->d_name) == "." || std::string(direntp->d_name) == "..") {
            continue;
        }

        std::string temp_path = path;
        temp_path.append("/").append(direntp->d_name);
        children_paths.push_back(temp_path);
        children_names.push_back(std::string(direntp->d_name));
        //std::cout << temp_path << std::endl;
    }

    closedir(dir);
    return true;
}

std::string
LocalTimeString(time_t t) {
    tm* local = localtime(&t); // to loal time
    char buf[128];
    memset(buf, 0, sizeof(buf));
    strftime(buf, 64, "%Y-%m-%d %H:%M:%S", local);
    return std::string(buf);
}

unsigned int
RSHash(const char *str) {
    unsigned int b = 378551;
    unsigned int a = 63689;
    unsigned int hash = 0;

    while (*str) {
        hash = hash * a + (*str++);
        a *= b;
    }
    return (hash & 0x7FFFFFFF);
}

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

bool
MakeDir(const std::string &path) {
    int ret = mkdir(path.c_str(), 0775);
    return ret == 0;
}

bool
RecurMakeDir(const std::string &path) {
    std::vector<std::string> sv;
    Split(path, '/', sv, "");

    bool b;
    std::string s = "/";
    for (auto &level : sv) {
        s.append(level);
        b = MakeDir(s);
        s.append("/");
    }
    return b;
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

std::string
IndexName(const std::string &table_name, const std::string &index_type, time_t timestamp) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s#%s#%lu", table_name.c_str(), index_type.c_str(), timestamp);
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
    return true;
}

bool ParseIndexName(const std::string &index_name, std::string &table_name, std::string &index_type, time_t &timestamp) {
    std::vector<std::string> sv;
    Split(index_name, '#', sv);
    if (sv.size() != 3) {
        return false;
    }
    table_name = sv[0];
    index_type = sv[1];
    sscanf(sv[2].c_str(), "%lu", &timestamp);
    return true;
}

bool
Distance(const std::vector<float> &v1, const std::vector<float> &v2, float &d) {
    if (v1.size() != v2.size()) {
        return false;
    }
    float pp = 0, qq = 0, pq = 0;
    for (size_t i = 0; i < v1.size(); ++i) {
        pp += v1[i] * v1[i];
        qq += v2[i] * v2[i];
        pq += v1[i] * v2[i];
    }
    float tmpd;
    float ppqq = pp * qq;
    if (ppqq > 0) {
        tmpd = 2.0 - 2.0 * pq / sqrt(ppqq);
    } else {
        tmpd = 2.0;
    }
    d = sqrt(std::max(tmpd, float(0)));
    return true;
}


} // namespace util

} // namespace vectordb
