#ifndef __VECTORDB_VDB_CLIENT_H__
#define __VECTORDB_VDB_CLIENT_H__

#include <memory>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include "vectordb_rpc.grpc.pb.h"
#include "status.h"

namespace vectordb {

class VdbClient {
  public:
    VdbClient(const std::string &address);
    ~VdbClient();
    VdbClient(const VdbClient&) = delete;
    VdbClient& operator=(const VdbClient&) = delete;
    Status Connect();

    Status Ping(const vectordb_rpc::PingRequest &request, vectordb_rpc::PingReply* reply);
    Status Info(const vectordb_rpc::InfoRequest &request, vectordb_rpc::InfoReply* reply);
    Status CreateTable(const vectordb_rpc::CreateTableRequest &request, vectordb_rpc::CreateTableReply* reply);
    Status ShowTables(const vectordb_rpc::ShowTablesRequest &request, vectordb_rpc::ShowTablesReply* reply);
    Status Describe(const vectordb_rpc::DescribeRequest &request, vectordb_rpc::DescribeReply* reply);
    Status PutVec(const vectordb_rpc::PutVecRequest &request, vectordb_rpc::PutVecReply* reply);
    Status GetVec(const vectordb_rpc::GetVecRequest &request, vectordb_rpc::GetVecReply* reply);
    Status DistKey(const vectordb_rpc::DistKeyRequest &request, vectordb_rpc::DistKeyReply* reply);
    Status Keys(const vectordb_rpc::KeysRequest &request, vectordb_rpc::KeysReply* reply);
    Status BuildIndex(const vectordb_rpc::BuildIndexRequest &request, vectordb_rpc::BuildIndexReply* reply);

    const std::string& address() const {
        return address_;
    }

  private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<vectordb_rpc::VectorDB::Stub> stub_;
    std::string address_;
};

} // namespace vectordb

#endif
