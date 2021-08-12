#include <glog/logging.h>
#include "config.h"
#include "coding.h"
#include "node.h"
#include "grpc_server.h"

namespace vectordb {

grpc::Status
VectorDBServiceImpl::Ping(grpc::ServerContext* context,
                          const vectordb_rpc::PingRequest* request,
                          vectordb_rpc::PingReply* reply) {
    auto s = Node::GetInstance().OnPing(request, reply);
    if (!s.ok()) {
        std::string msg = "Ping error: ";
        msg.append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::Info(grpc::ServerContext* context,
                          const vectordb_rpc::InfoRequest* request,
                          vectordb_rpc::InfoReply* reply) {
    auto s = Node::GetInstance().OnInfo(request, reply);
    if (!s.ok()) {
        std::string msg = "Info error: ";
        msg.append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::CreateTable(grpc::ServerContext* context,
                                 const vectordb_rpc::CreateTableRequest* request,
                                 vectordb_rpc::CreateTableReply* reply) {
    auto s = Node::GetInstance().OnCreateTable(request, reply);
    if (!s.ok()) {
        std::string msg = "CreateTable error, ";
        msg.append(request->table_name()).append(", ").append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
}


grpc::Status
VectorDBServiceImpl::ShowTables(grpc::ServerContext* context,
                                const vectordb_rpc::ShowTablesRequest* request,
                                vectordb_rpc::ShowTablesReply* reply) {
    auto s = Node::GetInstance().OnShowTables(request, reply);
    if (!s.ok()) {
        std::string msg = "ShowTables error: ";
        msg.append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::Describe(grpc::ServerContext* context,
                              const vectordb_rpc::DescribeRequest* request,
                              vectordb_rpc::DescribeReply* reply) {
    auto s = Node::GetInstance().OnDescribe(request, reply);
    if (!s.ok()) {
        std::string msg = "Describe error: ";
        msg.append(request->name()).append(", ").append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::PutVec(grpc::ServerContext* context,
                            const vectordb_rpc::PutVecRequest* request,
                            vectordb_rpc::PutVecReply* reply) {
    auto s = Node::GetInstance().OnPutVec(request, reply);
    if (!s.ok()) {
        std::string msg = "PutVec error: ";
        msg.append(request->table_name()).append(", ");
        msg.append(request->vec_obj().key()).append(", ").append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;

}

grpc::Status
VectorDBServiceImpl::GetVec(grpc::ServerContext* context,
                            const vectordb_rpc::GetVecRequest* request,
                            vectordb_rpc::GetVecReply* reply) {
    auto s = Node::GetInstance().OnGetVec(request, reply);
    if (!s.ok()) {
        std::string msg = "GetVec error: ";
        msg.append(request->table_name()).append(", ");
        msg.append(request->key()).append(", ").append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::DistKey(grpc::ServerContext* context,
                             const vectordb_rpc::DistKeyRequest* request,
                             vectordb_rpc::DistKeyReply* reply) {
    //auto s = Node::GetInstance().OnDistKey(request, reply);
    //assert(s.ok());
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::Keys(grpc::ServerContext* context,
                          const vectordb_rpc::KeysRequest* request,
                          vectordb_rpc::KeysReply* reply) {
    auto s = Node::GetInstance().OnKeys(request, reply);
    if (!s.ok()) {
        std::string msg = "Keys error: ";
        msg.append(request->table_name()).append(", ");
        msg.append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::BuildIndex(grpc::ServerContext* context,
                                const vectordb_rpc::BuildIndexRequest* request,
                                vectordb_rpc::BuildIndexReply* reply) {
    auto s = Node::GetInstance().OnBuildIndex(request, reply);
    if (!s.ok()) {
        std::string msg = "BuildIndex error: ";
        msg.append(request->table_name()).append(", ");
        msg.append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::GetKNN(grpc::ServerContext* context,
                            const vectordb_rpc::GetKNNRequest* request,
                            vectordb_rpc::GetKNNReply* reply) {
    auto s = Node::GetInstance().OnGetKNN(request, reply);
    if (!s.ok()) {
        std::string msg = "GetKNN error: ";
        msg.append(request->table_name()).append(", ").append(request->key()).append(", ");
        msg.append(s.ToString());
        LOG(INFO) << msg;
    }
    return grpc::Status::OK;
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

} // namespace vectordb
