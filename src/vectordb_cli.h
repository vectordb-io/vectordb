#ifndef __VECTORDB_CLI_H__
#define __VECTORDB_CLI_H__

#include <memory>
#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include "vectordb_rpc.grpc.pb.h"
#include "vdb_client.h"
#include "status.h"
#include "vindex.h"

namespace vectordb {

class VectordbCli {
  public:
    static VectordbCli&
    GetInstance() {
        static VectordbCli instance;
        return instance;
    }

    VectordbCli(const VectordbCli&) = delete;
    VectordbCli& operator=(const VectordbCli&) = delete;

    Status Init();
    Status Start();
    Status Stop();

  private:
    VectordbCli();
    ~VectordbCli();

    void Prompt() const;
    void ShowReply(const std::string &s) const;
    void Do(const std::vector<std::string> &cmd_sv, const std::string &params_json);

    Status PreProcess(const std::string &params_json, vectordb_rpc::PingRequest &request, std::string &reply_msg);
    Status PreProcess(const std::string &params_json, vectordb_rpc::InfoRequest &request, std::string &reply_msg);
    Status PreProcess(const std::string &params_json, vectordb_rpc::CreateTableRequest &request, std::string &reply_msg);
    Status PreProcess(const std::string &params_json, vectordb_rpc::ShowTablesRequest &request, std::string &reply_msg);
    Status PreProcess(const std::string &params_json, vectordb_rpc::PutVecRequest &request, std::string &reply_msg);
    Status PreProcess(const std::string &params_json, vectordb_rpc::GetVecRequest &request, std::string &reply_msg);
    Status PreProcess(const std::string &params_json, vectordb_rpc::KeysRequest &request, std::string &reply_msg);
    Status PreProcess(const std::string &params_json, vectordb_rpc::BuildIndexRequest &request, std::string &reply_msg);
    Status PreProcess(const std::string &params_json, vectordb_rpc::GetKNNRequest &request, std::string &reply_msg);

    void Ping(const vectordb_rpc::PingRequest &request, std::string &reply_msg);
    void Info(const vectordb_rpc::InfoRequest &request, std::string &reply_msg);
    void CreateTable(const vectordb_rpc::CreateTableRequest &request, std::string &reply_msg);
    void DropTable(const vectordb_rpc::DropTableRequest &request, std::string &reply_msg);
    void ShowTables(const vectordb_rpc::ShowTablesRequest &request, std::string &reply_msg);
    void Describe(const vectordb_rpc::DescribeRequest &request, std::string &reply_msg);
    void PutVec(const vectordb_rpc::PutVecRequest &request, std::string &reply_msg);
    void GetVec(const vectordb_rpc::GetVecRequest &request, std::string &reply_msg);
    void Keys(const vectordb_rpc::KeysRequest &request, std::string &reply_msg);
    void BuildIndex(const vectordb_rpc::BuildIndexRequest &request, std::string &reply_msg);
    void GetKNN(const vectordb_rpc::GetKNNRequest &request, std::string &reply_msg);



    void DistKey(const vectordb_rpc::DistKeyRequest &request, std::string &reply_msg);


  private:
    VdbClient vdb_client_;
};

} // namespace vectordb

#endif
