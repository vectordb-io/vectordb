#include <glog/logging.h>
#include "leveldb/db.h"
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
    tp.dim = request->dim();

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
                std::map<std::string, std::string> empty_indices;
                auto vengine = std::make_shared<VEngine>(replica_sp->path(), request->dim(), empty_indices);
                if (!vengine) {
                    reply->set_code(1);
                    std::string msg = "create table ";
                    msg.append(request->table_name());
                    msg.append(" error");
                    reply->set_msg(msg);
                    return Status::Corruption(msg);
                }
                auto s = vengine->Init();
                assert(s.ok());
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

Status
Node::OnPutVec(const vectordb_rpc::PutVecRequest* request, vectordb_rpc::PutVecReply* reply) {
    std::string replica_name;
    std::string err_msg;
    auto it = meta_.tables().find(request->table());
    if (it == meta_.tables().end()) {
        err_msg = "table not exist";
        reply->set_code(1);
        reply->set_msg(err_msg);
        return Status::OK();
    }

    if (it->second->engine_type() != VECTOR_ENGINE) {
        err_msg = "engine type error";
        reply->set_code(1);
        reply->set_msg(err_msg);
        return Status::OK();
    }

    auto s = meta_.ReplicaName(request->table(), request->vec_obj().key(), replica_name);
    if (!s.ok()) {
        err_msg = "get replica error";
        reply->set_code(1);
        reply->set_msg(err_msg);
        return Status::OK();
    }

    auto sp = engine_manager_.GetVEngine(replica_name);
    assert(sp);

    VecObj vo;
    Pb2VecObj(request->vec_obj(), vo);
    if (it->second->dim() != vo.vec().dim()) {
        err_msg = "dim error";
        reply->set_code(1);
        reply->set_msg(err_msg);
        return Status::OK();
    }

    s = sp->Put(request->vec_obj().key(), vo);
    if (!s.ok()) {
        err_msg = "db put error";
        reply->set_code(1);
        reply->set_msg(err_msg);
        return Status::OK();
    }

    err_msg = "put vector ok";
    reply->set_code(0);
    reply->set_msg(err_msg);
    return Status::OK();
}

Status
Node::OnGetVec(const vectordb_rpc::GetVecRequest* request, vectordb_rpc::GetVecReply* reply) {
    VecObj vo;
    auto s = GetVec(request->table(), request->key(), vo);
    if (!s.ok()) {
        reply->set_code(1);
        reply->set_msg(s.ToString());
    } else {
        reply->set_code(0);
        reply->set_msg("get vector ok");
        VecObj2Pb(vo, *(reply->mutable_vec_obj()));
    }
    return Status::OK();
}

Status
Node::OnDistKey(const vectordb_rpc::DistKeyRequest* request, vectordb_rpc::DistKeyReply* reply) {
    VecObj vo1, vo2;
    Status s;
    std::string err_msg;

    s = GetVec(request->table(), request->key1(), vo1);
    if (!s.ok()) {
        reply->set_code(1);
        reply->set_msg(s.ToString());
        return Status::OK();
    }

    s = GetVec(request->table(), request->key2(), vo2);
    if (!s.ok()) {
        reply->set_code(1);
        reply->set_msg(s.ToString());
        return Status::OK();
    }

    double distance;
    bool b = util::Distance(vo1.vec().data(), vo2.vec().data(), distance);
    assert(b);

    err_msg = "distance key ok";
    reply->set_code(0);
    reply->set_msg(err_msg);
    reply->set_distance(distance);
    return Status::OK();
}

Status
Node::GetVec(const std::string &table, const std::string &key, VecObj &vo) const {
    std::string replica_name;
    std::string err_msg;
    Status s;

    auto it = meta_.tables().find(table);
    if (it == meta_.tables().end()) {
        err_msg = "table not exist";
        return Status::Corruption(err_msg);
    }

    if (it->second->engine_type() != VECTOR_ENGINE) {
        err_msg = "engine type error";
        return Status::Corruption(err_msg);
    }

    s = meta_.ReplicaName(table, key, replica_name);
    if (!s.ok()) {
        err_msg = "get replica error";
        return Status::Corruption(err_msg);
    }

    auto sp = engine_manager_.GetVEngine(replica_name);
    assert(sp);

    s = sp->Get(key, vo);
    if (!s.ok()) {
        err_msg = "db get error, key:";
        err_msg.append(key);
        return Status::Corruption(err_msg);
    }
    return Status::OK();
}

Status
Node::OnKeys(const vectordb_rpc::KeysRequest* request, vectordb_rpc::KeysReply* reply) {
    //std::vector<std::string> keys;
    auto it = meta_.tables().find(request->table());
    if (it != meta_.tables().end()) {
        for (auto &partition_kv : it->second->partitions()) {
            for (auto &replica_kv : partition_kv.second->replicas()) {
                auto replica_sp = replica_kv.second;
                if (replica_sp->id() == 0) {
                    auto engine_sp = engine_manager_.GetVEngine(replica_sp->name());
                    assert(engine_sp);
                    leveldb::DB* db = engine_sp->data();
                    assert(db);

                    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
                    for (it->SeekToFirst(); it->Valid(); it->Next()) {
                        //keys.push_back(it->key().ToString());
                        reply->add_keys(it->key().ToString());
                    }
                    assert(it->status().ok());  // Check for any errors found during the scan
                    delete it;
                }
            }
        }
        reply->set_code(0);
        reply->set_msg("ok");

    } else {
        reply->set_code(1);
        reply->set_msg("table not exist");
    }

    return Status::OK();
}

Status
Node::OnBuildIndex(const vectordb_rpc::BuildIndexRequest* request, vectordb_rpc::BuildIndexReply* reply) {
    std::string err_msg;
    Status s;

    auto it = meta_.tables().find(request->table());
    if (it == meta_.tables().end()) {
        reply->set_code(1);
        reply->set_msg("table not exist");

    } else {
        std::string index_type = request->index_type();
        util::ToLower(index_type);
        std::string index_name;
        char buf[128];
        snprintf(buf, sizeof(buf), "%s%ld", index_type.c_str(), time(nullptr));
        index_name = std::string(buf);

        for (auto &partition_kv : it->second->partitions()) {
            for (auto &replica_kv : partition_kv.second->replicas()) {
                auto replica_sp = replica_kv.second;
                auto engine_sp = engine_manager_.GetVEngine(replica_sp->name());
                assert(engine_sp);

                auto s = engine_sp->AddIndex(index_name, index_type);
                assert(s.ok());
            }
        }

        it->second->mutable_indices().insert(std::pair<std::string, std::string>(index_name, index_type));
        meta_.Persist();

        reply->set_code(0);
        reply->set_msg("ok");
    }

    return Status::OK();
}

} // namespace vectordb
