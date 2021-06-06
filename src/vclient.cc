#include "util.h"
#include "cli_config.h"
#include "vclient.h"

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

        std::string line;
        std::getline(std::cin, line);
        LOG(INFO) << "[" << line << "]";

        std::vector<std::string> argv;
        Parse(line, argv);

        LOG(INFO) << "parse command:";
        for (auto &s : argv) {
            LOG(INFO) << s;
        }

        Do(argv);
    }
}

void
VClient::Do(std::vector<std::string> &argv) {
    if (argv.size() == 0) {
        return;
    }

    std::string command = argv[0];
    ToLower(command);

    if ("exit" == command || "quit" == command) {
        exit(0);
    }

    if ("ping" == command) {
        std::string reply = Ping();
        ShowReply(reply);
    } else {
        printf("unknown command [%s] \n", command.c_str());
    }

}

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

std::string
VClient::Ping() {
    vectordb_rpc::PingRequest request;
    request.set_msg("ping");
    vectordb_rpc::PingReply reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->Ping(&context, request, &reply);
    if (status.ok()) {
        return reply.msg();
    } else {
        LOG(ERROR) << status.error_code() << ": " << status.error_message();
        return "RPC failed";
    }
}

}  // namespace vectordb
