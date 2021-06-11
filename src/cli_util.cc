#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cassert>
#include <string>
#include <sstream>
#include <iostream>
#include "jsonxx/json.hpp"
#include "cli_util.h"

namespace vectordb {

namespace cli_util {

std::string
HelpStr() {
    std::string s;
    s.append("-------------------------------------------------------------------------\n");
    s.append("console command example:").append("\n").append("\n");

    s.append("help").append("\n");
    s.append("info").append("\n");
    s.append("ping").append("\n");
    s.append("exit").append("\n");
    s.append("quit").append("\n");
    s.append("version").append("\n");
    s.append("show tables").append("\n");
    s.append("describe table_name").append("\n");
    s.append("describe partition_name").append("\n");
    s.append("describe replica_name").append("\n").append("\n");

    s.append("create table {\"table_name\":\"vector_table\", \"engine_type\":\"vector\", \"dim\":100, \"partition_num\":1, \"replica_num\":1}").append("\n");
    s.append("put {\"table_name\":\"vector_table\", \"key\":\"kkk\", \"vector\":[1.13, 2.25, 3.73, 4.99], \"attach_value1\":\"attach_value1\", \"attach_value2\":\"attach_value2\", \"attach_value3\":\"attach_value3\"}").append("\n");
    s.append("build index {\"table_name\":\"vector_table\", \"index_type\":\"annoy\"}").append("\n");
    s.append("build index {\"table_name\":\"vector_table\", \"index_type\":\"knn_graph\", \"k\":0}").append("\n");
    s.append("get {\"table_name\":\"vector_table\", \"key\":\"kkk\"}").append("\n");
    s.append("getknn {\"table_name\":\"vector_table\", \"key\":\"kkk\", \"limit\":20}").append("\n");
    s.append("distance key {\"table_name\":\"vector_table\", \"key1\":\"xxx\", \"key2\":\"ooo\"}").append("\n");
    s.append("distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99], \"vector2\":[3.93, 9.27, 4.63, 2.91]}").append("\n").append("\n");

    s.append("create table {\"table_name\":\"kv_table\", \"engine_type\":\"kv\", \"partition_num\":1, \"replica_num\":1}").append("\n");
    s.append("put {\"table_name\":\"kv_table\", \"key\":\"kkk\", \"value\":\"vvv\"}").append("\n");
    s.append("get {\"table_name\":\"kv_table\", \"key\":\"kkk\"}").append("\n");
    s.append("del {\"table_name\":\"kv_table\", \"key\":\"kkk\"}").append("\n").append("\n");

    s.append("create table {\"table_name\":\"graph_table\", \"engine_type\":\"graph\", \"partition_num\":1, \"replica_num\":1}").append("\n");
    s.append("-------------------------------------------------------------------------\n");
    return s;
}

void
ToLower(std::string &str) {
    for (size_t i = 0; i < str.size(); i++)
        str[i] = tolower(str[i]);
}

std::string
ToString(const vectordb_rpc::PingReply &reply) {
    return reply.msg();
}

std::string
ToString(const vectordb_rpc::CreateTableReply &reply) {
    jsonxx::json j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    return j.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::ShowTablesReply &reply) {
    std::string s;
    for (int i = 0; i < reply.tables_size(); ++i) {
        s.append(reply.tables(i)).append("\n");
    }
    return s;
}

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

void
Split2(const std::string &s, char separator, std::string &s1, std::string &s2) {
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (*it == separator) {
            s1 = std::string(s.begin(), it);
            s2 = std::string(it, s.end());
            return;
        }
    }

    s1 = s;
    s2 = "";
}

} // namespace cli_util

} // namespace vectordb
