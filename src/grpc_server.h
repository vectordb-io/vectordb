#ifndef __VECTORDB_GRPC_SERVER_H__
#define __VECTORDB_GRPC_SERVER_H__

#include "vector_rpc.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include "status.h"

namespace vectordb {

class VectorDBServiceImpl final : public vector_rpc::VectorDB::Service {
  public:
    grpc::Status Ping(grpc::ServerContext* context,
                      const vector_rpc::PingRequest* request,
                      vector_rpc::PingReply* reply) override;
  private:
};

class GrpcServer {
  public:
    GrpcServer();
    ~GrpcServer();
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

}  // namespace vectordb

#endif
