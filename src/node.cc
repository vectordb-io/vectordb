#include <glog/logging.h>
#include "leveldb/db.h"
#include "vengine.h"
#include "vindex_knn_graph.h"
#include "coding.h"
#include "config.h"
#include "util.h"
#include "node.h"

namespace vectordb {

Node::Node()
    :meta_(Config::GetInstance().meta_path()) {
}

Node::~Node() {
}

Status
Node::Init() {
    util::RecurMakeDir(Config::GetInstance().data_path());
    if (!util::DirOK(Config::GetInstance().data_path())) {
        std::string msg = "data_dir error: ";
        msg.append(Config::GetInstance().data_path());
        return Status::OtherError(msg);
    }

    util::RecurMakeDir(Config::GetInstance().engine_path());
    if (!util::DirOK(Config::GetInstance().engine_path())) {
        std::string msg = "engine_dir error: ";
        msg.append(Config::GetInstance().engine_path());
        return Status::OtherError(msg);
    }

    auto s = meta_.Init();
    assert(s.ok());

    s = engine_manager_.Init();
    assert(s.ok());

    s = grpc_server_.Init();
    assert(s.ok());

    return Status::OK();
}

Status
Node::Start() {
    Status s;
    s = grpc_server_.Start();
    return s;
}

Status
Node::Stop() {
    Status s;
    s = grpc_server_.Stop();
    return s;
}

Status
Node::OnPing(const vectordb_rpc::PingRequest* request, vectordb_rpc::PingReply* reply) {
    if (request->msg() == "ping") {
        reply->set_msg("pang");
    } else {
        reply->set_msg("no_sound");
    }
    return Status::OK();
}

Status
Node::OnInfo(const vectordb_rpc::InfoRequest* request, vectordb_rpc::InfoReply* reply) {
    return Status::OK();
}

Status
Node::OnCreateTable(const vectordb_rpc::CreateTableRequest* request, vectordb_rpc::CreateTableReply* reply) {
    std::string table_path = Config::GetInstance().engine_path();
    table_path.append("/").append(request->table_name());
    std::string msg = "create table error: ";

    TableParam tp;
    tp.name = request->table_name();
    tp.partition_num = request->partition_num();
    tp.replica_num = request->replica_num();
    tp.path = table_path;
    tp.dim = request->dim();

    auto s = meta_.AddTable(tp);
    if (!s.ok()) {
        reply->set_code(1);
        msg.append(s.Msg());
        reply->set_msg(msg);
        LOG(INFO) << msg;
        return Status::OtherError(s.Msg());
    }

    auto table_sp = meta_.GetTable(request->table_name());
    assert(table_sp);

    s = meta_.ForEachReplica2(std::bind(&vectordb::EngineManager::AddVEngine3, &Node::GetInstance().mutable_engine_manager(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    if (!s.ok()) {
        reply->set_code(2);
        msg.append(s.Msg());
        reply->set_msg(msg);
        LOG(INFO) << msg;
        return Status::OtherError(s.Msg());
    }

    s = meta_.Persist();
    if (!s.ok()) {
        reply->set_code(3);
        msg.append(s.Msg());
        reply->set_msg(msg);
        LOG(INFO) << msg;
        return Status::OtherError(s.Msg());
    }

    reply->set_code(0);
    msg = "create table ";
    msg.append(request->table_name());
    msg.append(" ok");
    LOG(INFO) << msg;
    reply->set_msg(msg);
    return Status::OK();
}

Status
Node::OnShowTables(const vectordb_rpc::ShowTablesRequest* request, vectordb_rpc::ShowTablesReply* reply) {
    return Status::OK();
}

Status
Node::OnDescribe(const vectordb_rpc::DescribeRequest* request, vectordb_rpc::DescribeReply* reply) {
    return Status::OK();
}

Status
Node::OnPutVec(const vectordb_rpc::PutVecRequest* request, vectordb_rpc::PutVecReply* reply) {
    return Status::OK();
}

Status
Node::OnGetVec(const vectordb_rpc::GetVecRequest* request, vectordb_rpc::GetVecReply* reply) {
    return Status::OK();
}

Status
Node::OnDistKey(const vectordb_rpc::DistKeyRequest* request, vectordb_rpc::DistKeyReply* reply) {
    return Status::OK();
}

/*
Status
Node::GetVec(const std::string &table, const std::string &key, VecObj &vo) const {
    return Status::OK();
}
*/

Status
Node::OnKeys(const vectordb_rpc::KeysRequest* request, vectordb_rpc::KeysReply* reply) {
    return Status::OK();
}

Status
Node::Keys(std::vector<std::string> &keys) {
    return Status::OK();
}

Status
Node::OnBuildIndex(const vectordb_rpc::BuildIndexRequest* request, vectordb_rpc::BuildIndexReply* reply) {
    return Status::OK();
}

/*
void
Node::AppendVecDt(std::vector<VecDt> &dst, const std::vector<VecDt> &src) const {
}
*/

Status
Node::OnGetKNN(const vectordb_rpc::GetKNNRequest* request, vectordb_rpc::GetKNNReply* reply) {
    return Status::OK();
}

} // namespace vectordb
