#include "config.h"
#include "node.h"

namespace vectordb {

Node::Node()
    :meta_(Config::GetInstance().data_path()+std::string("/meta")) {
}

Node::~Node() {
}

Status
Node::Init() {
    Status s;
    s = grpc_server_.Init();
    assert(s.ok());
    return Status::OK();
}

Status
Node::Start() {
    Status s;
    s = grpc_server_.Start();
    assert(s.ok());
    return Status::OK();
}

Status
Node::Stop() {
    Status s;
    s = grpc_server_.Stop();
    assert(s.ok());
    return Status::OK();
}

Status
Node::OnPing(const vectordb_rpc::PingRequest* request, vectordb_rpc::PingReply* reply) {
    if (request->msg() == "ping") {
        reply->set_msg("pang");
    } else {
        reply->set_msg("no_sound");
    }
    return Status::OK();
}

}  // namespace vectordb
