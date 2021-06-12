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

    if (!util::DirOK(Config::GetInstance().engine_path())) {
        util::Mkdir(Config::GetInstance().engine_path());
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

    reply->set_code(1);
    reply->set_msg("create table error");

    TableParam tp;
    tp.name = request->table_name();
    tp.partition_num = request->partition_num();
    tp.replica_num = request->replica_num();
    tp.engine_type = request->engine_type();
    tp.path = table_path;

    auto s = meta_.AddTable(tp);
    if (!s.ok()) {
        reply->set_code(1);
        std::string msg = "create table ";
        msg.append(request->table_name());
        msg.append(" error");
        reply->set_msg(msg);
        return Status::Corruption(msg);
    }
    meta_.Persist();

    if (tp.engine_type == VECTOR_ENGINE) {
        auto it_table = meta_.tables().find(request->table_name());
        assert(it_table != meta_.tables().end());
        if (!util::DirOK(it_table->second->path())) {
            util::Mkdir(it_table->second->path());
        }

        for (auto &p : it_table->second->partitions()) {
            if (!util::DirOK(p.second->path())) {
                util::Mkdir(p.second->path());
            }

            for (auto &r : p.second->replicas()) {
                auto replica_sp = r.second;
                auto vengine = std::make_shared<VEngine>(replica_sp->path());
                s = vengine->Init();
                if (!s.ok()) {
                    reply->set_code(1);
                    std::string msg = "create table ";
                    msg.append(request->table_name());
                    msg.append(" error");
                    reply->set_msg(msg);
                    return Status::Corruption(msg);
                }
                engine_manager_.AddVEngine(replica_sp->name(), vengine);
            }
        }
    }

    meta_.Persist();
    reply->set_code(0);
    std::string msg = "create table ";
    msg.append(request->table_name());
    msg.append(" ok");
    reply->set_msg(msg);
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
