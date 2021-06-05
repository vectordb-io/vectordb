#include <glog/logging.h>
#include "config.h"
#include "node.h"
#include "grpc_server.h"

namespace vectordb {

grpc::Status
VectorDBServiceImpl::Ping(grpc::ServerContext* context,
                          const vectordb_rpc::PingRequest* request,
                          vectordb_rpc::PingReply* reply) {
    auto s = Node::GetInstance().OnPing(request, reply);
    assert(s.ok());

    return grpc::Status::OK;
}

GrpcServer::GrpcServer() {
}

GrpcServer::~GrpcServer() {
}

Status
GrpcServer::Init() {
    return Status::OK();
}

Status
GrpcServer::Start() {
    auto s = StartService();
    assert(s.ok());
    return Status::OK();
}

Status
GrpcServer::Stop() {
    auto s = StopService();
    assert(s.ok());
    return Status::OK();
}

Status
GrpcServer::StartService() {
    std::string server_address(Config::GetInstance().address().ToString());
    VectorDBServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    server_ = builder.BuildAndStart();
    LOG(INFO) << "Server listening on " << server_address;
    server_->Wait();
    return Status::OK();
}

Status GrpcServer::StopService() {
    server_->Shutdown();
    return Status::OK();
}

}  // namespace vectordb
