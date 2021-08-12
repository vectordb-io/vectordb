#ifndef __VECTORDB_GRPC_SERVER_H__
#define __VECTORDB_GRPC_SERVER_H__

#include "vectordb_rpc.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include "status.h"

namespace vectordb {

class VectorDBServiceImpl final : public vectordb_rpc::VectorDB::Service {
  public:
    grpc::Status Ping(grpc::ServerContext* context,
                      const vectordb_rpc::PingRequest* request,
                      vectordb_rpc::PingReply* reply) override;

    grpc::Status Info(grpc::ServerContext* context,
                      const vectordb_rpc::InfoRequest* request,
                      vectordb_rpc::InfoReply* reply) override;

    grpc::Status CreateTable(grpc::ServerContext* context,
                             const vectordb_rpc::CreateTableRequest* request,
                             vectordb_rpc::CreateTableReply* reply) override;

    grpc::Status DropTable(grpc::ServerContext* context,
                           const vectordb_rpc::DropTableRequest* request,
                           vectordb_rpc::DropTableReply* reply) override;

    grpc::Status ShowTables(grpc::ServerContext* context,
                            const vectordb_rpc::ShowTablesRequest* request,
                            vectordb_rpc::ShowTablesReply* reply) override;

    grpc::Status Describe(grpc::ServerContext* context,
                          const vectordb_rpc::DescribeRequest* request,
                          vectordb_rpc::DescribeReply* reply) override;

    grpc::Status PutVec(grpc::ServerContext* context,
                        const vectordb_rpc::PutVecRequest* request,
                        vectordb_rpc::PutVecReply* reply) override;

    grpc::Status GetVec(grpc::ServerContext* context,
                        const vectordb_rpc::GetVecRequest* request,
                        vectordb_rpc::GetVecReply* reply) override;

    grpc::Status DistKey(grpc::ServerContext* context,
                         const vectordb_rpc::DistKeyRequest* request,
                         vectordb_rpc::DistKeyReply* reply) override;

    grpc::Status Keys(grpc::ServerContext* context,
                      const vectordb_rpc::KeysRequest* request,
                      vectordb_rpc::KeysReply* reply) override;

    grpc::Status BuildIndex(grpc::ServerContext* context,
                            const vectordb_rpc::BuildIndexRequest* request,
                            vectordb_rpc::BuildIndexReply* reply) override;

    grpc::Status GetKNN(grpc::ServerContext* context,
                        const vectordb_rpc::GetKNNRequest* request,
                        vectordb_rpc::GetKNNReply* reply) override;

  private:
};

class GrpcServer {
  public:
    GrpcServer() {};
    ~GrpcServer() {};
    GrpcServer(const GrpcServer&) = delete;
    GrpcServer& operator=(const GrpcServer&) = delete;

    Status Init();
    Status Start();
    Status Stop();

  private:
    Status StartService();
    Status StopService();

    std::unique_ptr<grpc::Server> server_;
};

} // namespace vectordb

#endif
