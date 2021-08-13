#include "jsonxx/json.hpp"
#include "cli_util.h"
#include "cli_config.h"
#include "vectordb_cli.h"
#include "version.h"

namespace vectordb {

VectordbCli::VectordbCli()
    :vdb_client_(CliConfig::GetInstance().address()) {
}

VectordbCli::~VectordbCli() {
}

Status
VectordbCli::Init() {
    auto s = vdb_client_.Connect();
    if (!s.ok()) {
        printf("vdb_client connect error: %s \n", s.ToString().c_str());
        return Status::OtherError(s.Msg());
    }
    return Status::OK();
}

Status
VectordbCli::Start() {
    while (true) {
        Prompt();

        std::string cmd_line, cmd_line1, cmd_line2;
        std::getline(std::cin, cmd_line);
        //LOG(INFO) << "[" << line << "]";
        cli_util::ToLower(cmd_line);
        cli_util::DelTail(cmd_line, ';');

        cli_util::Split2(cmd_line, '{', cmd_line1, cmd_line2);
        std::vector<std::string> cmd_sv;
        cli_util::Split(cmd_line1, ' ', cmd_sv, "\t");
        Do(cmd_sv, cmd_line2);
    }
}

void
VectordbCli::Do(const std::vector<std::string> &cmd_sv, const std::string &params_json) {
    std::string reply;
    if (cmd_sv.size() == 0) {
        return;

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "create" && cmd_sv[1] == "table") {
        vectordb_rpc::CreateTableRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            CreateTable(request, reply);
        }

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "show" && cmd_sv[1] == "tables") {
        vectordb_rpc::ShowTablesRequest request;
        ShowTables(request, reply);

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "desc") {
        vectordb_rpc::DescribeRequest request;
        request.set_name(cmd_sv[1]);
        Describe(request, reply);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "ping") {
        vectordb_rpc::PingRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            Ping(request, reply);
        }

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "exit") {
        exit(0);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "exit()") {
        exit(0);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "exit;") {
        exit(0);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "quit") {
        exit(0);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "quit()") {
        exit(0);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "quit;") {
        exit(0);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "help") {
        reply = cli_util::HelpStr();

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "info") {
        vectordb_rpc::InfoRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            Info(request, reply);
        }

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "version") {
        reply = __VECTORDB__VERSION__;

    } else if (cmd_sv.size() == 3 && cmd_sv[0] == "build" && cmd_sv[1] == "index") {
        vectordb_rpc::BuildIndexRequest request;
        request.set_table_name(cmd_sv[2]);
        request.set_index_type(VINDEX_TYPE_ANNOY);
        request.set_distance_type(VINDEX_DISTANCE_TYPE_COSINE);
        request.mutable_annoy_param()->set_tree_num(20);
        request.mutable_knn_graph_param()->set_k(20);
        BuildIndex(request, reply);

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "build" && cmd_sv[1] == "index") {
        vectordb_rpc::BuildIndexRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            BuildIndex(request, reply);
        }

    } else if (cmd_sv.size() == 3 && cmd_sv[0] == "drop" && cmd_sv[1] == "table") {
        vectordb_rpc::DropTableRequest request;
        request.set_table_name(cmd_sv[2]);
        DropTable(request, reply);

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "drop" && cmd_sv[1] == "index") {
        vectordb_rpc::DropIndexRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            DropIndex(request, reply);
        }

    } else if (cmd_sv.size() == 3 && cmd_sv[0] == "drop" && cmd_sv[1] == "index") {
        vectordb_rpc::DropIndexRequest request;
        request.add_index_names(cmd_sv[2]);
        DropIndex(request, reply);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "keys") {
        vectordb_rpc::KeysRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            Keys(request, reply);
        }

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "put") {
        vectordb_rpc::PutVecRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            PutVec(request, reply);
        }

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "distance" && cmd_sv[1] == "key") {
        try {
            auto j = jsonxx::json::parse(params_json);
            std::string table_name = j["table_name"].as_string();
            std::string key1 = j["key1"].as_string();
            std::string key2 = j["key2"].as_string();
            vectordb_rpc::DistKeyRequest request;
            request.set_table_name(table_name);
            request.set_key1(key1);
            request.set_key2(key2);
            DistKey(request, reply);

        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "get") {
        vectordb_rpc::GetVecRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            GetVec(request, reply);
        }

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "getknn") {
        vectordb_rpc::GetKNNRequest request;
        auto s = PreProcess(params_json, request, reply);
        if (s.ok()) {
            GetKNN(request, reply);
        }

    } else {
        reply.append("unknown command: ");
        for (auto &s : cmd_sv) {
            reply.append(s).append(" ");
        }
        reply.append(params_json);
    }
    ShowReply(reply);

}

Status
VectordbCli::Stop() {
    return Status::OK();
}

void
VectordbCli::ShowReply(const std::string &s) const {
    printf("%s \n", s.c_str());
    fflush(nullptr);
}

void
VectordbCli::Prompt() const {
    printf("(vector-cli) %s> ", CliConfig::GetInstance().address().c_str());
    fflush(nullptr);
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::ShowTablesRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    return Status::OK();
}

void
VectordbCli::ShowTables(const vectordb_rpc::ShowTablesRequest &request, std::string &reply_msg) {
    vectordb_rpc::ShowTablesReply reply;
    auto s = vdb_client_.ShowTables(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}


Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::CreateTableRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    do {
        std::string table_name;
        int partition_num;
        int replica_num;
        int dim;
        jsonxx::json j;

        try {
            j = jsonxx::json::parse(params_json);
        } catch (std::exception &e) {
            reply_msg = "parameters error";
            break;
        }

        // optional value
        try {
            partition_num = j["partition_num"].as_integer();
        } catch (std::exception &e) {
            partition_num = 1;
        }

        // optional value
        try {
            replica_num = j["replica_num"].as_integer();
        } catch (std::exception &e) {
            replica_num = 1;
        }

        // required value
        try {
            table_name = j["table_name"].as_string();
        } catch (std::exception &e) {
            reply_msg = "table_name error, ";
            reply_msg.append(e.what());
            break;
        }

        // required value
        try {
            dim = j["dim"].as_integer();
        } catch (std::exception &e) {
            reply_msg = "dim error, ";
            reply_msg.append(e.what());
            break;
        }

        request.set_table_name(table_name);
        request.set_partition_num(partition_num);
        request.set_replica_num(replica_num);
        request.set_dim(dim);
        return Status::OK();

    } while (0);

    return Status::OtherError(reply_msg);
}

void
VectordbCli::CreateTable(const vectordb_rpc::CreateTableRequest &request, std::string &reply_msg) {
    vectordb_rpc::CreateTableReply reply;
    auto s = vdb_client_.CreateTable(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

void
VectordbCli::DropTable(const vectordb_rpc::DropTableRequest &request, std::string &reply_msg) {
    vectordb_rpc::DropTableReply reply;
    auto s = vdb_client_.DropTable(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::PingRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    request.set_msg("ping");
    return Status::OK();
}

void
VectordbCli::Ping(const vectordb_rpc::PingRequest &request, std::string &reply_msg) {
    vectordb_rpc::PingReply reply;
    auto s = vdb_client_.Ping(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

void
VectordbCli::Describe(const vectordb_rpc::DescribeRequest &request, std::string &reply_msg) {
    vectordb_rpc::DescribeReply reply;
    auto s = vdb_client_.Describe(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::InfoRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    return Status::OK();
}

void
VectordbCli::Info(const vectordb_rpc::InfoRequest &request, std::string &reply_msg) {
    vectordb_rpc::InfoReply reply;
    auto s = vdb_client_.Info(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::PutVecRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    do {
        std::string table_name;
        std::string key;
        std::string attach_value1;
        std::string attach_value2;
        std::string attach_value3;
        jsonxx::json j;

        try {
            j = jsonxx::json::parse(params_json);
        } catch (std::exception &e) {
            reply_msg = "parameters error";
            break;
        }

        // optional value
        try {
            attach_value1 = j["attach_value1"].as_string();
        } catch (std::exception &e) {
            attach_value1 = "";
        }

        // optional value
        try {
            attach_value2 = j["attach_value2"].as_string();
        } catch (std::exception &e) {
            attach_value2 = "";
        }

        // optional value
        try {
            attach_value3 = j["attach_value3"].as_string();
        } catch (std::exception &e) {
            attach_value3 = "";
        }

        // required value
        try {
            table_name = j["table_name"].as_string();
        } catch (std::exception &e) {
            reply_msg = "table_name error, ";
            reply_msg.append(e.what());
            break;
        }

        // required value
        try {
            key = j["key"].as_string();
        } catch (std::exception &e) {
            reply_msg = "key error, ";
            reply_msg.append(e.what());
            break;
        }

        // required value
        try {
            bool b = j["vector"].is_array();
            if (!b) {
                reply_msg = "vector error";
                break;
            }
        } catch (std::exception &e) {
            reply_msg = "vector error, ";
            reply_msg.append(e.what());
            break;
        }
        const auto& vector_arr = j["vector"].as_array();

        request.set_table_name(table_name);
        request.mutable_vec_obj()->set_key(key);
        request.mutable_vec_obj()->set_attach_value1(attach_value1);
        request.mutable_vec_obj()->set_attach_value2(attach_value2);
        request.mutable_vec_obj()->set_attach_value3(attach_value3);
        for (auto &jobj : vector_arr) {
            float df = jobj.as_float();
            request.mutable_vec_obj()->mutable_vec()->add_data(df);
        }
        return Status::OK();

    } while(0);

    return Status::OtherError(reply_msg);
}

void
VectordbCli::PutVec(const vectordb_rpc::PutVecRequest &request, std::string &reply_msg) {
    vectordb_rpc::PutVecReply reply;
    auto s = vdb_client_.PutVec(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::GetVecRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    do {
        jsonxx::json j;
        std::string table_name;
        std::string key;

        try {
            j = jsonxx::json::parse(params_json);
        } catch (std::exception &e) {
            reply_msg = "parameters error";
            break;
        }

        // required value
        try {
            table_name = j["table_name"].as_string();
            key = j["key"].as_string();

        } catch (std::exception &e) {
            reply_msg = "get error, ";
            reply_msg.append(e.what());
            break;
        }

        request.set_table_name(table_name);
        request.set_key(key);
        return Status::OK();

    } while (0);

    return Status::OtherError(reply_msg);
}

void
VectordbCli::GetVec(const vectordb_rpc::GetVecRequest &request, std::string &reply_msg) {
    vectordb_rpc::GetVecReply reply;
    auto s = vdb_client_.GetVec(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

void
VectordbCli::DistKey(const vectordb_rpc::DistKeyRequest &request, std::string &reply_msg) {

    /*
    vectordb_rpc::DistKeyReply reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->DistKey(&context, request, &reply);
    if (status.ok()) {
    reply_msg = cli_util::ToString(reply);
    } else {
    reply_msg = status.error_message();
    }
    */
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::KeysRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    do {
        jsonxx::json j;
        std::string table_name;
        int begin, limit;

        try {
            j = jsonxx::json::parse(params_json);
        } catch (std::exception &e) {
            reply_msg = "parameters error";
            break;
        }

        // required value
        try {
            table_name = j["table_name"].as_string();

        } catch (std::exception &e) {
            reply_msg = "keys error, ";
            reply_msg.append(e.what());
            break;
        }

        // optional value
        try {
            begin = j["begin"].as_integer();
        } catch (std::exception &e) {
            begin = 0;
        }

        // optional value
        try {
            limit = j["limit"].as_integer();
        } catch (std::exception &e) {
            limit = 200;
        }

        request.set_table_name(table_name);
        request.set_begin(begin);
        request.set_limit(limit);
        return Status::OK();

    } while (0);

    return Status::OtherError(reply_msg);
}

void
VectordbCli::Keys(const vectordb_rpc::KeysRequest &request, std::string &reply_msg) {
    vectordb_rpc::KeysReply reply;
    auto s = vdb_client_.Keys(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::DropIndexRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    do {
        jsonxx::json j;

        try {
            j = jsonxx::json::parse(params_json);
        } catch (std::exception &e) {
            reply_msg = "parameters error";
            break;
        }

        // required value
        try {
            bool b = j["index_names"].is_array();
            if (!b) {
                reply_msg = "index_names error";
                break;
            }
        } catch (std::exception &e) {
            reply_msg = "index_names error, ";
            reply_msg.append(e.what());
            break;
        }
        const auto& index_names_arr = j["index_names"].as_array();

        for (auto &jobj : index_names_arr) {
            std::string index_name = jobj.as_string();
            request.add_index_names(index_name);
        }

        return Status::OK();

    } while (0);

    return Status::OtherError(reply_msg);
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::BuildIndexRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    do {
        jsonxx::json j;
        std::string table_name;
        std::string index_type;
        std::string distance_type;
        int tree_num, k;

        try {
            j = jsonxx::json::parse(params_json);
        } catch (std::exception &e) {
            reply_msg = "parameters error";
            break;
        }

        // required value
        try {
            table_name = j["table_name"].as_string();
        } catch (std::exception &e) {
            reply_msg = "table_name error, ";
            reply_msg.append(e.what());
            break;
        }

        // optional value
        try {
            index_type = j["index_type"].as_string();
        } catch (std::exception &e) {
            index_type = VINDEX_TYPE_ANNOY;
        }

        // optional value
        try {
            distance_type = j["distance_type"].as_string();
        } catch (std::exception &e) {
            distance_type = VINDEX_DISTANCE_TYPE_COSINE;
        }

        // optional value
        try {
            k = j["k"].as_integer();
        } catch (std::exception &e) {
            k = 20;
        }

        // optional value
        try {
            tree_num = j["tree_num"].as_integer();
        } catch (std::exception &e) {
            tree_num = 20;
        }

        request.set_table_name(table_name);
        request.set_index_type(index_type);
        request.set_distance_type(distance_type);
        request.mutable_annoy_param()->set_tree_num(tree_num);
        request.mutable_knn_graph_param()->set_k(k);
        return Status::OK();

    } while (0);

    return Status::OtherError(reply_msg);
}

void
VectordbCli::BuildIndex(const vectordb_rpc::BuildIndexRequest &request, std::string &reply_msg) {
    vectordb_rpc::BuildIndexReply reply;
    auto s = vdb_client_.BuildIndex(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

void
VectordbCli::DropIndex(const vectordb_rpc::DropIndexRequest &request, std::string &reply_msg) {
    vectordb_rpc::DropIndexReply reply;
    auto s = vdb_client_.DropIndex(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

Status
VectordbCli::PreProcess(const std::string &params_json, vectordb_rpc::GetKNNRequest &request, std::string &reply_msg) {
    reply_msg.clear();
    do {
        jsonxx::json j;
        std::string table_name;
        std::string key;
        int limit;
        std::string index_name;

        try {
            j = jsonxx::json::parse(params_json);
        } catch (std::exception &e) {
            reply_msg = "parameters error";
            break;
        }

        // required value
        try {
            table_name = j["table_name"].as_string();
        } catch (std::exception &e) {
            reply_msg = "table_name error, ";
            reply_msg.append(e.what());
            break;
        }

        // required value
        try {
            key = j["key"].as_string();
        } catch (std::exception &e) {
            reply_msg = "key error, ";
            reply_msg.append(e.what());
            break;
        }

        // optional value
        try {
            limit = j["limit"].as_integer();
        } catch (std::exception &e) {
            limit = 20;
        }

        // optional value
        try {
            index_name = j["index_name"].as_string();
        } catch (std::exception &e) {
            index_name = "default";
        }

        request.set_table_name(table_name);
        request.set_key(key);
        request.set_limit(limit);
        request.set_index_name(index_name);
        return Status::OK();

    } while (0);

    return Status::OtherError(reply_msg);
}

void
VectordbCli::GetKNN(const vectordb_rpc::GetKNNRequest &request, std::string &reply_msg) {
    vectordb_rpc::GetKNNReply reply;
    auto s = vdb_client_.GetKNN(request, &reply);
    if (s.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = s.Msg();
    }
}

} // namespace vectordb
