#include "jsonxx/json.hpp"
#include "cli_util.h"
#include "cli_config.h"
#include "vclient.h"
#include "version.h"

namespace vectordb {

VClient::VClient() {
}

VClient::~VClient() {
}

Status
VClient::Init() {
    channel_ = grpc::CreateChannel(CliConfig::GetInstance().address(), grpc::InsecureChannelCredentials());
    stub_ = vectordb_rpc::VectorDB::NewStub(channel_);

    return Status::OK();
}

Status
VClient::Start() {
    while (true) {
        Prompt();

        std::string cmd_line, cmd_line1, cmd_line2;
        std::getline(std::cin, cmd_line);
        //LOG(INFO) << "[" << line << "]";
        cli_util::ToLower(cmd_line);

        cli_util::Split2(cmd_line, '{', cmd_line1, cmd_line2);
        std::vector<std::string> cmd_sv;
        cli_util::Split(cmd_line1, ' ', cmd_sv, "\t");
        Do(cmd_sv, cmd_line2);
    }
}

void
VClient::Do(const std::vector<std::string> &cmd_sv, const std::string &params_json) {
    std::string reply;
    if (cmd_sv.size() == 0) {
        return;

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "create" && cmd_sv[1] == "table") {
        std::cout << "cmd: create table" << std::endl;
        try {
            auto j = jsonxx::json::parse(params_json);
            std::string table_name = j["table_name"].as_string();
            std::string engine_type = j["engine_type"].as_string();
            int dim = j["dim"].as_integer();

            std::cout << "table_name:" << table_name << std::endl;
            std::cout << "engine_type:" << engine_type << std::endl;
            std::cout << "dim:" << dim << std::endl;

        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "show" && cmd_sv[1] == "tables") {

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "describe") {

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "ping") {
        Ping(reply);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "exit") {
        exit(0);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "quit") {
        exit(0);

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "help") {
        reply = cli_util::HelpStr();

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "info") {


    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "version") {
        reply = __VECTORDB__VERSION__;

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "build" && cmd_sv[1] == "index") {

    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "put" && cmd_sv[1] == "vector") {
        std::cout << "cmd: put vector" << std::endl;
        try {
            auto j = jsonxx::json::parse(params_json);
            std::string table_name = j["table_name"].as_string();
            std::string key = j["key"].as_string();
            std::string attach_value1 = j["attach_value1"].as_string();
            std::string attach_value2 = j["attach_value2"].as_string();
            std::string attach_value3 = j["attach_value3"].as_string();
            const auto& vector_arr = j["vector"].as_array();

            std::cout << "table_name:" << table_name << std::endl;
            std::cout << "key:" << key << std::endl;
            std::cout << "vector: {" << std::endl;
            for (auto &jobj : vector_arr) {
                double dd;
                dd = jobj.as_float();
                std::cout << "jobj.is_float(): " << jobj.is_float() << " data:" << dd << std::endl;
            }
            std::cout << "}" << std::endl;
            std::cout << "attach_value1:" << attach_value1 << std::endl;
            std::cout << "attach_value2:" << attach_value2 << std::endl;
            std::cout << "attach_value3:" << attach_value3 << std::endl;


        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }


    } else if (cmd_sv.size() == 2 && cmd_sv[0] == "get" && cmd_sv[1] == "vector") {

    } else if (cmd_sv.size() == 1 && cmd_sv[0] == "getknn") {

    } else {
        reply.append("unknown command: ");
        for (auto &s : cmd_sv) {
            reply.append(s).append(" ");
        }
        reply.append(params_json);
    }
    ShowReply(reply);

}

/*
void
VClient::Do(std::vector<std::string> &argv) {
    if (argv.size() == 0) {
        return;
    }

    Status s;
    std::string reply;
    std::string command = argv[0];
    cli_util::ToLower(command);

    if ("exit" == command || "quit" == command) {
        exit(0);
    }

    if ("ping" == command) {
        s = Ping(reply);
        assert(s.ok());

    } else if("help" == command) {
        reply = cli_util::HelpStr();

    } else if("info" == command) {
        s = Info(reply);
        assert(s.ok());

    } else if("version" == command) {
        reply = __VECTORDB__VERSION__;

    } else {
        reply = "unknown command: ";
        reply.append(command);
    }

    ShowReply(reply);
}
*/

Status
VClient::Stop() {
    return Status::OK();
}

void
VClient::ShowReply(const std::string &s) const {
    printf("%s \n", s.c_str());
    fflush(nullptr);
}

void
VClient::Prompt() const {
    printf("(vector-cli) %s> ", CliConfig::GetInstance().address().c_str());
    fflush(nullptr);
}

void
VClient::Ping(std::string &reply_msg) {
    vectordb_rpc::PingRequest request;
    request.set_msg("ping");
    vectordb_rpc::PingReply reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->Ping(&context, request, &reply);
    if (status.ok()) {
        reply_msg = cli_util::ToString(reply);
    } else {
        reply_msg = status.error_message();
    }
}

void
VClient::Info(std::string &reply_msg) {
    vectordb_rpc::InfoRequest request;
    vectordb_rpc::InfoReply reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->Info(&context, request, &reply);
    if (status.ok()) {
        reply_msg = reply.msg();
    } else {
        reply_msg = status.error_message();
    }
}

}  // namespace vectordb
