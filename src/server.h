#ifndef __VECTORDB_SERVER_H__
#define __VECTORDB_SERVER_H__

#include "status.h"
#include "grpc_server.h"

namespace vectordb {

class Server {
  public:
    static Server&
    GetInstance() {
        static Server instance;
        return instance;
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Status Init();
    Status Start();
    Status Stop();

    Status OnPing(const vector_rpc::PingRequest* request, vector_rpc::PingReply* reply);

    const GrpcServer* grpc_server() const {
        return &grpc_server_;
    }

  private:
    Server();
    ~Server();

    GrpcServer grpc_server_;
};

}  // namespace vectordb

#endif
