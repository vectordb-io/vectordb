#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cassert>
#include <string>
#include <sstream>
#include <iostream>
#include "cli_util.h"
#include "usage_string.h"

namespace vectordb {

namespace cli_util {

std::string
LocalTimeString(time_t t) {
    tm* local = localtime(&t); // to loal time
    char buf[128];
    memset(buf, 0, sizeof(buf));
    strftime(buf, 64, "%Y-%m-%d %H:%M:%S", local);
    return std::string(buf);
}

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
    std::string s(command_help_str);
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
    jsonxx::json64 j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    return j.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::DropTableReply &reply) {
    jsonxx::json64 j;
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
    jsonxx::json64 j, jt;
    int k = 0;
    for (int i = 0; i < reply.tables_size(); ++i) {
        j[k++] = reply.tables(i);
    }
    jt["tables"] = j;
    return jt.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::DescribeReply &reply) {
    jsonxx::json64 j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();

    if (reply.describe_table()) {
        j["table"] = ToJson(reply.table_name());
    }
    if (reply.describe_partition()) {
        j["partition"] = ToJson(reply.partition());
    }
    if (reply.describe_replica()) {
        j["replica"] = ToJson(reply.replica());
    }
    return j.dump(4, ' ');
}

jsonxx::json64
ToJson(const vectordb_rpc::Table &table) {
    jsonxx::json64 j;
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
        std::string index_name = table.indices(i);

        std::vector<std::string> sv;
        Split(index_name, '#', sv, " \t");
        if (sv.size() == 3) {
            j["indices"][k]["name"] = index_name;
            j["indices"][k]["type"] = sv[1];
            time_t t;
            sscanf(sv[2].c_str(), "%lu", &t);
            j["indices"][k]["create_time"] = LocalTimeString(t);

        } else {
            j["indices"][k] = table.indices(i);
        }
        k++;
    }
    return j;
}

jsonxx::json64
ToJson(const vectordb_rpc::Partition &partition) {
    jsonxx::json64 j;
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

jsonxx::json64
ToJson(const vectordb_rpc::Replica &replica) {
    jsonxx::json64 j;
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
    jsonxx::json64 j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    return j.dump(4, ' ');
}

jsonxx::json64
ToJson(const vectordb_rpc::VecObj &vec_obj) {
    jsonxx::json64 j;
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
    jsonxx::json64 j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    j["vec_obj"] = ToJson(reply.vec_obj());
    return j.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::DistKeyReply &reply) {
    jsonxx::json64 j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    j["distance"] = reply.distance();
    return j.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::KeysReply &reply) {
    jsonxx::json64 j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    for (int i = 0; i < reply.keys_size(); ++i) {
        j["keys"][i] = reply.keys(i);
    }
    return j.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::BuildIndexReply &reply) {
    jsonxx::json64 j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    return j.dump(4, ' ');
}

std::string
ToString(const vectordb_rpc::GetKNNReply &reply) {
    jsonxx::json64 j;
    j["code"] = reply.code();
    j["msg"] = reply.msg();
    for (int i = 0; i < reply.vecdts_size(); ++i) {
        jsonxx::json64 jvdt;
        jvdt["key"] = reply.vecdts(i).key();
        jvdt["distance"] = reply.vecdts(i).distance();
        jvdt["attach_value1"] = reply.vecdts(i).attach_value1();
        jvdt["attach_value2"] = reply.vecdts(i).attach_value2();
        jvdt["attach_value3"] = reply.vecdts(i).attach_value3();
        j["vecdts"][i] = jvdt;
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
