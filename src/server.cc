#include "server.h"

namespace vectordb {

Server::Server() {
}

Server::~Server() {
}

Status
Server::Init() {
    Status s;
    s = grpc_server_.Init();
    assert(s.ok());
    return Status::OK();
}

Status
Server::Start() {
    Status s;
    s = grpc_server_.Start();
    assert(s.ok());
    return Status::OK();
}

Status
Server::Stop() {
    Status s;
    s = grpc_server_.Stop();
    assert(s.ok());
    return Status::OK();
}

Status
Server::OnPing(const vector_rpc::PingRequest* request, vector_rpc::PingReply* reply) {
    if (request->msg() == "ping") {
        reply->set_msg("pang");
    } else {
        reply->set_msg("no_sound");
    }
    return Status::OK();
}

}  // namespace vectordb
