#include <glog/logging.h>
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
    Status s;
    if (!util::DirOK(Config::GetInstance().data_path())) {
        util::Mkdir(Config::GetInstance().data_path());
    }

    s = meta_.Init();
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
    assert(s.ok());
    return Status::OK();
}

Status
Node::Stop() {
    Status s;
    s = grpc_server_.Stop();
    assert(s.ok());
    return Status::OK();
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
    reply->set_msg(meta_.ToStringPretty());
    return Status::OK();
}

Status
Node::OnCreateTable(const vectordb_rpc::CreateTableRequest* request, vectordb_rpc::CreateTableReply* reply) {
    std::string table_path = Config::GetInstance().engine_path();
    table_path.append("/").append(request->table_name());
    EngineType et = StringToEngineType(request->engine_type());

    reply->set_code(1);
    reply->set_msg("create table error");

    auto s = meta_.AddTable(
                 request->table_name(),
                 request->partition_num(),
                 request->replica_num(),
                 et,
                 table_path);
    if (s.ok()) {
        meta_.Persist();
        reply->set_code(0);
        reply->set_msg("create table ok");
    }

    if (et == kVectorEngine) {
        VEngine* vengine;
    }

    return Status::OK();
}

Status
Node::OnShowTables(const vectordb_rpc::ShowTablesRequest* request, vectordb_rpc::ShowTablesReply* reply) {
    std::vector<std::string> tables;
    for (auto &t : meta_.tables()) {
        tables.push_back(t.first);
    }

    for (auto &t : tables) {
        std::string *s = reply->add_tables();
        *s = t;
    }

    return Status::OK();
}

Status
Node::OnDescribe(const vectordb_rpc::DescribeRequest* request, vectordb_rpc::DescribeReply* reply) {
    reply->set_code(0);
    std::string msg = "desc ";
    msg.append(request->name()).append(" ok");
    reply->set_msg(msg);

    std::shared_ptr<Table> pt = meta_.GetTable(request->name());
    if (pt) {
        reply->set_describe_table(true);
        Table2Pb(*pt, *(reply->mutable_table()));
    } else {
        reply->set_describe_table(false);
    }

    std::shared_ptr<Partition> pp = meta_.GetPartition(request->name());
    if (pp) {
        reply->set_describe_partition(true);
        Partition2Pb(*pp, *(reply->mutable_partition()));
    } else {
        reply->set_describe_partition(false);
    }

    std::shared_ptr<Replica> pr = meta_.GetReplica(request->name());
    if (pr) {
        reply->set_describe_replica(true);
        Replica2Pb(*pr, *(reply->mutable_replica()));
    } else {
        reply->set_describe_replica(false);
    }

    return Status::OK();
}



} // namespace vectordb
