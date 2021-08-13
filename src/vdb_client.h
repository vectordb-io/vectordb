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

    // simply call
    Status Ping(vectordb_rpc::PingReply* reply);
    Status CreateTable(const std::string &table_name, int dim, vectordb_rpc::CreateTableReply* reply);
    Status DropTable(const std::string &table_name, vectordb_rpc::DropTableReply* reply);
    Status DropIndex(const std::vector<std::string> &index_names, vectordb_rpc::DropIndexReply* reply);
    Status LeaveIndex(const std::string &table_name, uint32_t left, vectordb_rpc::LeaveIndexReply* reply);
    Status PutVec(const std::string &table_name,
                  const std::string &key,
                  const std::vector<float> &vec,
                  const std::vector<std::string> &attach_values,
                  vectordb_rpc::PutVecReply* reply);
    Status BuildIndex(const std::string &table_name, vectordb_rpc::BuildIndexReply* reply);
    Status GetKNN(const std::string &table_name, const std::string &key, int limit, vectordb_rpc::GetKNNReply* reply);
    Status DistVec(const std::vector<float> &vec1, const std::vector<float> &vec2, vectordb_rpc::DistVecReply* reply);
    Status DistKey(const std::string &table_name,
                   const std::string &key1,
                   const std::string &key2,
                   const std::string &distance_type,
                   vectordb_rpc::DistKeyReply* reply);

    // original grpc call
    Status Ping(const vectordb_rpc::PingRequest &request, vectordb_rpc::PingReply* reply);
    Status Info(const vectordb_rpc::InfoRequest &request, vectordb_rpc::InfoReply* reply);
    Status CreateTable(const vectordb_rpc::CreateTableRequest &request, vectordb_rpc::CreateTableReply* reply);
    Status DropTable(const vectordb_rpc::DropTableRequest &request, vectordb_rpc::DropTableReply* reply);
    Status DropIndex(const vectordb_rpc::DropIndexRequest &request, vectordb_rpc::DropIndexReply* reply);
    Status LeaveIndex(const vectordb_rpc::LeaveIndexRequest &request, vectordb_rpc::LeaveIndexReply* reply);
    Status ShowTables(const vectordb_rpc::ShowTablesRequest &request, vectordb_rpc::ShowTablesReply* reply);
    Status Describe(const vectordb_rpc::DescribeRequest &request, vectordb_rpc::DescribeReply* reply);
    Status PutVec(const vectordb_rpc::PutVecRequest &request, vectordb_rpc::PutVecReply* reply);
    Status GetVec(const vectordb_rpc::GetVecRequest &request, vectordb_rpc::GetVecReply* reply);
    Status DistKey(const vectordb_rpc::DistKeyRequest &request, vectordb_rpc::DistKeyReply* reply);
    Status Keys(const vectordb_rpc::KeysRequest &request, vectordb_rpc::KeysReply* reply);
    Status BuildIndex(const vectordb_rpc::BuildIndexRequest &request, vectordb_rpc::BuildIndexReply* reply);
    Status GetKNN(const vectordb_rpc::GetKNNRequest &request, vectordb_rpc::GetKNNReply* reply);
    Status DistVec(const vectordb_rpc::DistVecRequest &request, vectordb_rpc::DistVecReply* reply);

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
