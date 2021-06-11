#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cassert>
#include <string>
#include <sstream>
#include <iostream>
#include "cli_util.h"

namespace vectordb {

std::string
HelpStr() {
    std::string s;
    s.append("\n");
    s.append("info \n");
    s.append("version \n");
    s.append("show tables \n");
    s.append("create table {table_name:\"xxx\", engine_type:\"annoy\", dim:256} \n");
    return s;
}

void ToLower(std::string &str) {
    for (size_t i = 0; i < str.size(); i++)
        str[i] = tolower(str[i]);
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

std::string
ToString(const vectordb_rpc::PingReply &msg) {
    return msg.msg();
}

}  // namespace vectordb
