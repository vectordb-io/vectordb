#ifndef __VECTORDB_NODE_H__
#define __VECTORDB_NODE_H__

#include "meta.h"
#include "status.h"
#include "vengine.h"
#include "engine_manager.h"
#include "grpc_server.h"

namespace vectordb {

class Node {
  public:
    static Node&
    GetInstance() {
        static Node instance;
        return instance;
    }

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Status Init();
    Status Start();
    Status Stop();

    Status OnPing(const vectordb_rpc::PingRequest* request, vectordb_rpc::PingReply* reply);
    Status OnInfo(const vectordb_rpc::InfoRequest* request, vectordb_rpc::InfoReply* reply);

    GrpcServer& grpc_server() {
        return grpc_server_;
    }

    EngineManager& engine_manager() {
        return engine_manager_;
    }

  private:
    Node();
    ~Node();

    Meta meta_;
    EngineManager engine_manager_;
    GrpcServer grpc_server_;
};

}  // namespace vectordb

#endif
