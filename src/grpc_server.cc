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
    assert(s.ok());
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::Info(grpc::ServerContext* context,
                          const vectordb_rpc::InfoRequest* request,
                          vectordb_rpc::InfoReply* reply) {
    auto s = Node::GetInstance().OnInfo(request, reply);
    assert(s.ok());
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::CreateTable(grpc::ServerContext* context,
                                 const vectordb_rpc::CreateTableRequest* request,
                                 vectordb_rpc::CreateTableReply* reply) {
    std::string table_path = Config::GetInstance().engine_path();
    table_path.append("/").append(request->table_name());
    EngineType et = StringToEngineType(request->engine_type());

    reply->set_code(1);
    reply->set_msg("create table error");

    auto s = Node::GetInstance().meta().AddTable(
                 request->table_name(),
                 request->partition_num(),
                 request->replica_num(),
                 et,
                 table_path);
    if (s.ok()) {
        Node::GetInstance().meta().Persist();
        reply->set_code(0);
        reply->set_msg("create table ok");
    }

    if (et == kVectorEngine) {
        // creaete vector db
    }

    return grpc::Status::OK;
}


grpc::Status
VectorDBServiceImpl::ShowTables(grpc::ServerContext* context,
                                const vectordb_rpc::ShowTablesRequest* request,
                                vectordb_rpc::ShowTablesReply* reply) {
    std::vector<std::string> tables;
    Node::GetInstance().meta().table_names(tables);
    for (auto &t : tables) {
        std::string *s = reply->add_tables();
        *s = t;
    }
    return grpc::Status::OK;
}

grpc::Status
VectorDBServiceImpl::Describe(grpc::ServerContext* context,
                              const vectordb_rpc::DescribeRequest* request,
                              vectordb_rpc::DescribeReply* reply) {
    reply->set_code(0);
    std::string msg = "describe ";
    msg.append(request->name()).append(" ok");
    reply->set_msg(msg);

    std::shared_ptr<Table> pt = Node::GetInstance().meta().GetTable(request->name());
    if (pt) {
        reply->set_describe_table(true);
        Table2Pb(*pt, *(reply->mutable_table()));
    } else {
        reply->set_describe_table(false);
    }

    std::shared_ptr<Partition> pp = Node::GetInstance().meta().GetPartition(request->name());
    if (pp) {
        reply->set_describe_partition(true);
        Partition2Pb(*pp, *(reply->mutable_partition()));
    } else {
        reply->set_describe_partition(false);
    }

    std::shared_ptr<Replica> pr = Node::GetInstance().meta().GetReplica(request->name());
    if (pr) {
        reply->set_describe_replica(true);
        Replica2Pb(*pr, *(reply->mutable_replica()));
    } else {
        reply->set_describe_replica(false);
    }

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
