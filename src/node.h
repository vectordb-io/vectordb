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
    Status GetVec(const std::string &table, const std::string &key, VecObj &vo) const;

    Status OnPing(const vectordb_rpc::PingRequest* request, vectordb_rpc::PingReply* reply);
    Status OnInfo(const vectordb_rpc::InfoRequest* request, vectordb_rpc::InfoReply* reply);
    Status OnCreateTable(const vectordb_rpc::CreateTableRequest* request, vectordb_rpc::CreateTableReply* reply);
    Status OnShowTables(const vectordb_rpc::ShowTablesRequest* request, vectordb_rpc::ShowTablesReply* reply);
    Status OnDescribe(const vectordb_rpc::DescribeRequest* request, vectordb_rpc::DescribeReply* reply);
    Status OnPutVec(const vectordb_rpc::PutVecRequest* request, vectordb_rpc::PutVecReply* reply);
    Status OnGetVec(const vectordb_rpc::GetVecRequest* request, vectordb_rpc::GetVecReply* reply);
    Status OnDistKey(const vectordb_rpc::DistKeyRequest* request, vectordb_rpc::DistKeyReply* reply);
    Status OnKeys(const vectordb_rpc::KeysRequest* request, vectordb_rpc::KeysReply* reply);
    Status OnBuildIndex(const vectordb_rpc::BuildIndexRequest* request, vectordb_rpc::BuildIndexReply* reply);
    Status OnGetKNN(const vectordb_rpc::GetKNNRequest* request, vectordb_rpc::GetKNNReply* reply);
    Status Keys(std::vector<std::string> &keys);

    GrpcServer& grpc_server() {
        return grpc_server_;
    }

    const EngineManager& engine_manager() const {
        return engine_manager_;
    }

    EngineManager& mutable_engine_manager() {
        return engine_manager_;
    }

    const Meta& meta() const {
        return meta_;
    }

    Meta& mutable_meta() {
        return meta_;
    }

  private:
    void AppendVecDt(std::vector<VecDt> &dst, const std::vector<VecDt> &src) const;

    Node();
    ~Node();

    Meta meta_;
    EngineManager engine_manager_;
    GrpcServer grpc_server_;
};

} // namespace vectordb

#endif
