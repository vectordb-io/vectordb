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
    //LOG(INFO) << meta_.ToStringPretty();
    reply->set_msg(meta_.ToStringPretty());
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

    s = meta_.ForEachReplicaOfTable(request->table_name(), std::bind(&vectordb::EngineManager::AddVEngine3, &Node::GetInstance().mutable_engine_manager(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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
Node::OnDropTable(const vectordb_rpc::DropTableRequest* request, vectordb_rpc::DropTableReply* reply) {
    std::string msg;
    std::shared_ptr<Table> table_sp = meta_.GetTable(request->table_name());
    if (!table_sp) {
        msg = "table not exist: ";
        msg.append(request->table_name());
        reply->set_code(1);
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    std::vector<std::string> replica_names;
    auto s = meta_.ReplicaNamesByTable(request->table_name(), replica_names);
    if (!s.ok()) {
        msg = "drop table meta_.ReplicaNamesByTable error " + request->table_name() + " " + s.Msg();
        reply->set_code(2);
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    s = meta_.DropTable(request->table_name());
    if (!s.ok()) {
        msg = "meta drop table " + request->table_name() + " error: " + s.Msg();
        reply->set_code(3);
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    for (auto &replica_name : replica_names) {
        s = engine_manager_.DelEngine(replica_name);
        if (!s.ok()) {
            msg = "drop table DelEngine error, " + replica_name + ", " + s.Msg();
            reply->set_code(4);
            reply->set_msg(msg);
            return Status::OtherError(msg);
        }
    }

    auto b = util::RemoveDir(table_sp->path());
    if (!b) {
        msg = "drop table rm dir error, " + table_sp->path();
        reply->set_code(5);
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    reply->set_code(0);
    msg = "drop table " + request->table_name() + " ok";
    reply->set_msg(msg);

    return Status::OK();
}

Status
Node::OnShowTables(const vectordb_rpc::ShowTablesRequest* request, vectordb_rpc::ShowTablesReply* reply) {
    std::map<std::string, std::shared_ptr<Table>> tables = meta_.tables_copy();
    for (auto &kv : tables) {
        std::string *s = reply->add_tables();
        *s = kv.first;
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
        coding::Table2Pb(*pt, *(reply->mutable_table_name()));
    } else {
        reply->set_describe_table(false);
    }

    std::shared_ptr<Partition> pp = meta_.GetPartition(request->name());
    if (pp) {
        reply->set_describe_partition(true);
        coding::Partition2Pb(*pp, *(reply->mutable_partition()));
    } else {
        reply->set_describe_partition(false);
    }

    std::shared_ptr<Replica> pr = meta_.GetReplica(request->name());
    if (pr) {
        reply->set_describe_replica(true);
        coding::Replica2Pb(*pr, *(reply->mutable_replica()));
    } else {
        reply->set_describe_replica(false);
    }

    return Status::OK();
}

Status
Node::OnPutVec(const vectordb_rpc::PutVecRequest* request, vectordb_rpc::PutVecReply* reply) {
    std::string replica_name;
    std::string msg;

    std::shared_ptr<Table> pt = meta_.GetTable(request->table_name());
    if (!pt) {
        msg = "table not exist: ";
        msg.append(request->table_name());
        reply->set_code(1);
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    auto s = meta_.ReplicaNameByKey(request->table_name(), request->vec_obj().key(), replica_name);
    if (!s.ok()) {
        msg = "get replica_name by key error: ";
        msg.append(replica_name).append(", ").append(s.Msg());
        LOG(INFO) << msg;
        reply->set_code(2);
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    auto vengine_sp = engine_manager_.GetVEngine(replica_name);
    if (!vengine_sp) {
        msg = "get vengine error: ";
        msg.append(replica_name).append(", ").append(s.Msg());;
        LOG(INFO) << msg;
        reply->set_code(3);
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    VecObj vo;
    coding::Pb2VecObj(request->vec_obj(), vo);
    if (pt->dim() != vo.vec().dim()) {
        msg = "dim not equal";
        LOG(INFO) << msg;
        reply->set_code(4);
        reply->set_msg(msg);
        return Status::OK();
    }

    s = vengine_sp->Put(request->vec_obj().key(), vo);
    if (!s.ok()) {
        msg = "db put error, ";
        msg.append(replica_name).append(", ");
        msg.append(request->vec_obj().key()).append(", ").append(s.Msg());;
        LOG(INFO) << msg;
        reply->set_code(5);
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    msg = "put vector ok";
    reply->set_code(0);
    reply->set_msg(msg);
    return Status::OK();
}

Status
Node::OnGetVec(const vectordb_rpc::GetVecRequest* request, vectordb_rpc::GetVecReply* reply) {
    VecObj vo;
    auto s = GetVec(request->table_name(), request->key(), vo);
    if (!s.ok()) {
        reply->set_code(1);
        reply->set_msg(s.Msg());
    } else {
        reply->set_code(0);
        reply->set_msg("get vector ok");
        coding::VecObj2Pb(vo, *(reply->mutable_vec_obj()));
    }
    return Status::OK();
}

Status
Node::GetVec(const std::string &table_name, const std::string &key, VecObj &vo) const {
    std::string replica_name;
    std::string msg;

    std::shared_ptr<Table> pt = meta_.GetTable(table_name);
    if (!pt) {
        msg = "table not exist: ";
        msg.append(table_name);
        return Status::OtherError(msg);
    }

    auto s = meta_.ReplicaNameByKey(table_name, key, replica_name);
    if (!s.ok()) {
        msg = "get replica_name by key error: ";
        msg.append(replica_name).append(", ").append(s.Msg());
        return Status::OtherError(msg);
    }

    auto vengine_sp = engine_manager_.GetVEngine(replica_name);
    if (!vengine_sp) {
        msg = "get vengine error: ";
        msg.append(replica_name).append(", ").append(s.Msg());;
        return Status::OtherError(msg);
    }

    s = vengine_sp->Get(key, vo);
    if (!s.ok()) {
        msg = "db get error, key: ";
        msg.append(key);
        return Status::OtherError(msg);
    }

    return Status::OK();
}

Status
Node::OnDistKey(const vectordb_rpc::DistKeyRequest* request, vectordb_rpc::DistKeyReply* reply) {
    return Status::OK();
}

Status
Node::OnKeys(const vectordb_rpc::KeysRequest* request, vectordb_rpc::KeysReply* reply) {
    std::vector<std::string> replica_names;
    auto s = meta_.ReplicaNamesByTable(request->table_name(), replica_names);
    if (!s.ok()) {
        std::string msg = "get replica_names error: ";
        msg.append(s.Msg());
        LOG(INFO) << msg;
        return Status::OtherError(msg);
    }

    int count = 0;
    int begin = 0;
    for (auto &replica_name : replica_names) {
        auto engine_sp = engine_manager_.GetVEngine(replica_name);
        if (!engine_sp) {
            std::string msg = "get engine error, ";
            msg.append(replica_name);
            LOG(INFO) << msg;
            return Status::OtherError(msg);
        }
        LOG(INFO) << "OnKeys, get engine: " << engine_sp->ToString();

        leveldb::DB* db = engine_sp->mutable_db_data();
        if (!db) {
            std::string msg = "db error, ";
            msg.append(replica_name);
            LOG(INFO) << msg;
            return Status::OtherError(msg);
        }

        leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            //char log_buf[128];
            //snprintf(log_buf, sizeof(log_buf), "begin:%d count:%d request->begin():%d request->limit():%d", begin, count, request->begin(), request->limit());
            //LOG(INFO) << log_buf;

            if (begin >= request->begin()) {
                //LOG(INFO) << "add key: " << it->key().ToString();
                reply->add_keys(it->key().ToString());
                count++;
                if (count >= request->limit()) {
                    delete it;
                    reply->set_code(0);
                    reply->set_msg("ok");
                    return Status::OK();
                }
            }
            begin++;
        }
        delete it;
    }

    reply->set_code(0);
    reply->set_msg("ok");
    return Status::OK();
}

Status
Node::Keys(std::vector<std::string> &keys) {
    return Status::OK();
}

Status
Node::OnBuildIndex(const vectordb_rpc::BuildIndexRequest* request, vectordb_rpc::BuildIndexReply* reply) {
    auto table_sp = meta_.GetTable(request->table_name());
    if (!table_sp) {
        reply->set_code(1);
        std::string msg = "table not exist:[";
        msg.append(request->table_name());
        msg.append("]");
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    int partition_num = table_sp->partition_num();
    //int replica_num = table_sp->replica_num();
    int replica_num = 1;
    time_t timestamp = time(nullptr);

    for (int partition_id = 0; partition_id < partition_num; ++partition_id) {
        for (int replica_id = 0; replica_id < replica_num; ++replica_id) {
            std::string replica_name = util::ReplicaName(request->table_name(), partition_id, replica_id);
            auto vengine_sp = engine_manager_.GetVEngine(replica_name);
            if (!vengine_sp) {
                reply->set_code(2);
                std::string msg = "get vengine error, " + replica_name;
                reply->set_msg(msg);
                return Status::OtherError(msg);
            }

            if (request->index_type() == VINDEX_TYPE_ANNOY) {
                AnnoyParam param;
                param.dim = vengine_sp->dim();
                param.index_type = request->index_type();
                param.distance_type = request->distance_type();
                param.replica_name = replica_name;
                param.timestamp = timestamp;
                param.tree_num = request->annoy_param().tree_num();

                auto s = vengine_sp->AddIndex(request->index_type(), &param);
                if (!s.ok()) {
                    reply->set_code(3);
                    std::string msg = s.Msg();
                    reply->set_msg(msg);
                    return Status::OtherError(msg);

                } else {
                    std::string msg = replica_name + " build index ok";
                    LOG(INFO) << msg;
                }

            } else {
                reply->set_code(4);
                std::string msg = "index type not support: " + request->index_type();
                reply->set_msg(msg);
                return Status::OtherError(msg);
            }
        }
    }

    std::string index_name = util::IndexName(request->table_name(), request->index_type(), timestamp);
    table_sp->AddIndexName(index_name);
    auto s = meta_.Persist();
    if (!s.ok()) {
        reply->set_code(5);
        std::string msg = "persist meta error, " + s.Msg();
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }
    LOG(INFO) << meta_.ToStringPretty();

    reply->set_code(0);
    std::string msg = "build index ok";
    reply->set_msg(msg);
    return Status::OK();
}

Status
Node::OnGetKNN(const vectordb_rpc::GetKNNRequest* request, vectordb_rpc::GetKNNReply* reply) {
    if (request->limit() < 0 || request->limit() > 1000) {
        reply->set_code(1);
        std::string msg = "limit error";
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    auto table_sp = meta_.GetTable(request->table_name());
    if (!table_sp) {
        reply->set_code(2);
        std::string msg = "table not exist:[";
        msg.append(request->table_name());
        msg.append("]");
        reply->set_msg(msg);
        return Status::OtherError(msg);
    }

    int partition_num = table_sp->partition_num();
    //int replica_num = table_sp->replica_num();
    int replica_num = 1;

    std::string distance_type;
    std::vector<VecDt> results;
    for (int partition_id = 0; partition_id < partition_num; ++partition_id) {
        for (int replica_id = 0; replica_id < replica_num; ++replica_id) {
            std::string replica_name = util::ReplicaName(request->table_name(), partition_id, replica_id);
            auto vengine_sp = engine_manager_.GetVEngine(replica_name);
            if (!vengine_sp) {
                reply->set_code(3);
                std::string msg = "get vengine error, " + replica_name;
                reply->set_msg(msg);
                return Status::OtherError(msg);
            }

            VecObj vo;
            auto s = GetVec(request->table_name(), request->key(), vo);
            if (!s.ok()) {
                std::string msg = s.Msg();
                reply->set_code(4);
                reply->set_msg(msg);
                return Status::OtherError(msg);
            }

            std::vector<VecDt> tmp_results;
            s = vengine_sp->GetKNN(vo.vec().data(), request->limit(), tmp_results, request->index_name());
            if (!s.ok()) {
                reply->set_code(5);
                std::string msg = s.Msg();
                reply->set_msg(msg);
                return Status::OtherError(msg);
            }

            auto index_sp = vengine_sp->mutable_vindex_manager().GetByName(request->index_name());
            if (!index_sp) {
                reply->set_code(6);
                std::string msg = "get index error, " + request->index_name();
                reply->set_msg(msg);
                return Status::OtherError(msg);
            } else {
                LOG(INFO) << "OnGetKNN get index: " << index_sp->ToString();
            }
            distance_type = index_sp->distance_type();

            results.insert(results.begin(), tmp_results.begin(), tmp_results.end());
            LOG(INFO) << replica_name << " tmp_results: ";
            for (auto &vdt : tmp_results) {
                LOG(INFO) << vdt.ToString();
            }
        }
    }

    if (distance_type == VINDEX_DISTANCE_TYPE_COSINE) {
        std::sort(results.begin(), results.end(), std::greater<VecDt>());
    } else {
        std::sort(results.begin(), results.end(), std::less<VecDt>());
    }

    LOG(INFO) << "results: ";
    for (auto &vdt : results) {
        LOG(INFO) << vdt.ToString();
    }

    int count = 0;
    for (auto &vdt : results) {
        if (count < request->limit()) {
            vectordb_rpc::VecDt* p = reply->add_vecdts();
            p->set_key(vdt.key());
            p->set_distance(vdt.distance());
            p->set_attach_value1(vdt.attach_value1());
            p->set_attach_value2(vdt.attach_value2());
            p->set_attach_value3(vdt.attach_value3());
            count++;

        } else {
            break;
        }
    }
    reply->set_code(0);
    reply->set_msg("ok");

    return Status::OK();
}

} // namespace vectordb
