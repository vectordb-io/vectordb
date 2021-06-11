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

    Status s;
    std::string reply;
    std::string command = argv[0];
    ToLower(command);

    if ("exit" == command || "quit" == command) {
        exit(0);
    }

    if ("ping" == command) {
        s = Ping(reply);
        assert(s.ok());

    } else if("help" == command) {
        reply = HelpStr();

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

Status
VClient::Ping(std::string &reply_msg) {
    vectordb_rpc::PingRequest request;
    request.set_msg("ping");
    vectordb_rpc::PingReply reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->Ping(&context, request, &reply);
    if (status.ok()) {
        reply_msg = ToString(reply);
    } else {
        reply_msg = status.error_message();
    }
    return Status::OK();
}

Status
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
    return Status::OK();
}

}  // namespace vectordb
