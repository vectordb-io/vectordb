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

void DelTail(std::string &s, char c) {
    while (true) {
        if (s.empty()) {
            return;
        }
        auto it = s.rbegin();
        if (*it == c) {
            s.pop_back();
        } else {
            break;
        }
    }
}

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
    s.append("desc table_name").append("\n");
    s.append("desc partition_name").append("\n");
    s.append("desc replica_name").append("\n").append("\n");

    s.append("create table {\"table_name\":\"vector_table\", \"engine_type\":\"vector\", \"dim\":4, \"partition_num\":10, \"replica_num\":1}").append("\n");
    s.append("put {\"table_name\":\"vector_table\", \"key\":\"kkk\", \"vector\":[1.13, 2.25, 3.73, 4.99], \"attach_value1\":\"attach_value1\", \"attach_value2\":\"attach_value2\", \"attach_value3\":\"attach_value3\"}").append("\n");
    s.append("build index {\"table_name\":\"vector_table\", \"index_type\":\"annoy\"}").append("\n");
    s.append("build index {\"table_name\":\"vector_table\", \"index_type\":\"knn_graph\", \"k\":0}").append("\n");
    s.append("get {\"table_name\":\"vector_table\", \"key\":\"kkk\"}").append("\n");
    s.append("getknn {\"table_name\":\"vector_table\", \"key\":\"kkk\", \"limit\":20}").append("\n");
    s.append("distance key {\"table_name\":\"vector_table\", \"key1\":\"xxx\", \"key2\":\"ooo\"}").append("\n");
    s.append("distance vector {\"vector1\":[1.13, 2.25, 3.73, 4.99], \"vector2\":[3.93, 9.27, 4.63, 2.91]}").append("\n").append("\n");
    s.append("keys {\"table_name\":\"kv_table\"}").append("\n");

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
ToString(const vectordb_rpc::InfoReply &reply) {
    return reply.msg();
}

std::string
ToString(const vectordb_rpc::ShowTablesReply &reply) {
    jsonxx::json j, jt;
    int k = 0;
    for (int i = 0; i < reply.tables_size(); ++i) {
        j[k++] = reply.tables(i);
    }
    jt["tables"] = j;
    return jt.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::DescribeReply &reply) {
    jsonxx::json j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();

    if (reply.describe_table()) {
        j["table"] = ToJson(reply.table());
    }
    if (reply.describe_partition()) {
        j["partition"] = ToJson(reply.partition());
    }
    if (reply.describe_replica()) {
        j["replica"] = ToJson(reply.replica());
    }
    return j.dump(4, ' ');
}

jsonxx::json
ToJson(const vectordb_rpc::Table &table) {
    jsonxx::json j;
    j["name"] = table.name();
    j["dim"] = table.dim();
    j["partition_num"] = table.partition_num();
    j["replica_num"] = table.replica_num();
    j["engine_type"] = table.engine_type();
    j["path"] = table.path();
    int k = 0;
    for (int i = 0; i < table.partitions_size(); ++i) {
        j["partitions"][k++] = table.partitions(i).name();
    }

    k = 0;
    for (int i = 0; i < table.indices_size(); ++i) {
        jsonxx::json ji;
        ji["index_name"] = table.indices(i).index_name();
        ji["index_type"] = table.indices(i).index_type();
        j["indices"][k++] = ji;
    }
    return j;
}

jsonxx::json
ToJson(const vectordb_rpc::Partition &partition) {
    jsonxx::json j;
    j["id"] = partition.id();
    j["name"] = partition.name();
    j["table_name"] = partition.table_name();
    j["replica_num"] = partition.replica_num();
    j["path"] = partition.path();
    int k = 0;
    for (int i = 0; i < partition.replicas_size(); ++i) {
        j["replicas"][k++] = partition.replicas(i).name();
    }
    return j;
}

jsonxx::json
ToJson(const vectordb_rpc::Replica &replica) {
    jsonxx::json j;
    j["id"] = replica.id();
    j["name"] = replica.name();
    j["table_name"] = replica.table_name();
    j["partition_name"] = replica.partition_name();
    j["address"] = replica.address();
    j["path"] = replica.path();
    return j;
}

std::string
ToString(const vectordb_rpc::PutVecReply &reply) {
    jsonxx::json j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    return j.dump(4, ' ');
}

jsonxx::json
ToJson(const vectordb_rpc::VecObj &vec_obj) {
    jsonxx::json j;
    j["key"] = vec_obj.key();
    j["attach_value1"] = vec_obj.attach_value1();
    j["attach_value2"] = vec_obj.attach_value2();
    j["attach_value3"] = vec_obj.attach_value3();
    for (int i = 0; i < vec_obj.vec().data_size(); ++i) {
        j["vec"][i] = vec_obj.vec().data(i);
    }
    return j;
}

std::string
ToString(const vectordb_rpc::GetVecReply &reply) {
    jsonxx::json j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    j["vec_obj"] = ToJson(reply.vec_obj());
    return j.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::DistKeyReply &reply) {
    jsonxx::json j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    j["distance"] = reply.distance();
    return j.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::KeysReply &reply) {
    jsonxx::json j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    for (int i = 0; i < reply.keys_size(); ++i) {
        j["keys"][i] = reply.keys(i);
    }
    return j.dump(4, ' ');
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
