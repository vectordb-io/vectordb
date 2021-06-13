#include "coding.h"

namespace vectordb {

void
Pb2Replica(const vectordb_rpc::Replica &pb, Replica &replica) {
    replica.set_id(pb.id());
    replica.set_name(pb.name());
    replica.set_table_name(pb.table_name());
    replica.set_partition_name(pb.partition_name());
    replica.set_path(pb.path());
}

void
Pb2Partition(const vectordb_rpc::Partition &pb, Partition &partition) {
    partition.set_id(pb.id());
    partition.set_name(pb.name());
    partition.set_table_name(pb.table_name());
    partition.set_replica_num(pb.replica_num());
    partition.set_path(pb.path());
    for (int i = 0; i < pb.replicas_size(); i++) {
        const vectordb_rpc::Replica &replica_pb = pb.replicas(i);
        Replica replica;
        Pb2Replica(replica_pb, replica);

        auto it = partition.replicas().find(replica.name());
        assert(it == partition.replicas().end());
        auto sp = std::make_shared<Replica>(replica);
        partition.mutable_replicas().insert(std::pair<std::string, std::shared_ptr<Replica>>(sp->name(), sp));
    }
}

void
Pb2Table(const vectordb_rpc::Table &pb, Table &table) {
    TableParam param;
    param.name = pb.name();
    param.partition_num = pb.partition_num();
    param.replica_num = pb.replica_num();
    param.engine_type = pb.engine_type();
    param.path = pb.path();
    param.dim = pb.dim();
    for (int i = 0; i < pb.partitions_size(); i++) {
        const vectordb_rpc::Partition &partition_pb = pb.partitions(i);
        Partition partition;
        Pb2Partition(partition_pb, partition);

        auto it = table.partitions().find(partition.name());
        assert(it == table.partitions().end());
        auto sp = std::make_shared<Partition>(partition);
        table.mutable_partitions().insert(std::pair<std::string, std::shared_ptr<Partition>>(sp->name(), sp));
    }

    for (int i = 0; i < pb.indices_size(); i++) {
        const vectordb_rpc::Index &index = pb.indices(i);
        table.mutable_indices().insert(std::pair<std::string, std::string>(index.index_name(), index.index_type()));
    }
}

void
Replica2Pb(const Replica &replica, vectordb_rpc::Replica &pb) {
    pb.set_id(replica.id());
    pb.set_name(replica.name());
    pb.set_table_name(replica.table_name());
    pb.set_partition_name(replica.partition_name());
    pb.set_address(replica.address());
    pb.set_path(replica.path());
}

void
Partition2Pb(const Partition &partition, vectordb_rpc::Partition &pb) {
    pb.set_id(partition.id());
    pb.set_name(partition.name());
    pb.set_table_name(partition.table_name());
    pb.set_replica_num(partition.replica_num());
    pb.set_path(partition.path());
    for (auto &r : partition.replicas()) {
        vectordb_rpc::Replica* replica = pb.add_replicas();
        Replica2Pb(*(r.second), *replica);
    }
}

void
Table2Pb(const Table &table, vectordb_rpc::Table &pb) {
    pb.set_name(table.name());
    pb.set_partition_num(table.partition_num());
    pb.set_replica_num(table.replica_num());
    pb.set_engine_type(table.engine_type());
    pb.set_path(table.path());
    pb.set_dim(table.dim());
    for (auto &p : table.partitions()) {
        vectordb_rpc::Partition* partition = pb.add_partitions();
        Partition2Pb(*(p.second), *partition);
    }

    for (auto &indices_ : table.indices()) {
        vectordb_rpc::Index* index = pb.add_indices();
        index->set_index_name(indices_.first);
        index->set_index_type(indices_.second);
    }
}

void
Replica2Str(const Replica &replica, std::string &s) {
    vectordb_rpc::Replica pb;
    Replica2Pb(replica, pb);
    bool ret = pb.SerializeToString(&s);
    assert(ret);
}

void
Partition2Str(const Partition &partition, std::string &s) {
    vectordb_rpc::Partition pb;
    Partition2Pb(partition, pb);
    bool ret = pb.SerializeToString(&s);
    assert(ret);
}

void
Table2Str(const Table &table, std::string &s) {
    vectordb_rpc::Table pb;
    Table2Pb(table, pb);
    bool ret = pb.SerializeToString(&s);
    assert(ret);
}

bool
Str2Replica(const std::string &s, Replica &replica) {
    vectordb_rpc::Replica pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        Pb2Replica(pb, replica);
    }
    return ret;
}

bool
Str2Partition(const std::string &s, Partition &partition) {
    vectordb_rpc::Partition pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        Pb2Partition(pb, partition);
    }
    return ret;
}

bool
Str2Table(const std::string &s, Table &table) {
    vectordb_rpc::Table pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        Pb2Table(pb, table);
    }
    return ret;
}

bool
Str2TableNames(const std::string &s, std::vector<std::string> &table_names) {
    vectordb_rpc::TableNames pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        for (int i = 0; i < pb.table_names_size(); i++) {
            const std::string &table_name = pb.table_names(i);
            table_names.push_back(table_name);
        }
    }
    return ret;
}

void
TableNames2Str(const std::vector<std::string> &table_names, std::string &s) {
    vectordb_rpc::TableNames pb;
    for (auto &name : table_names) {
        std::string *s = pb.add_table_names();
        *s = name;
    }
    bool ret = pb.SerializeToString(&s);
    assert(ret);
}

void
Vec2Pb(const Vec &v, vectordb_rpc::Vec &pb) {
    for (auto &d : v.data()) {
        pb.add_data(d);
    }
}

void
Pb2Vec(const vectordb_rpc::Vec &pb, Vec &v) {
    v.mutable_data().clear();
    for (int i = 0; i < pb.data_size(); ++i) {
        v.mutable_data().push_back(pb.data(i));
    }
}

void
Vec2Str(const Vec &v, std::string &s) {
    vectordb_rpc::Vec pb;
    Vec2Pb(v, pb);
    bool ret = pb.SerializeToString(&s);
    assert(ret);
}

bool
Str2Vec(const std::string &s, Vec &v) {
    vectordb_rpc::Vec pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        Pb2Vec(pb, v);
    }
    return ret;
}

} // namespace vectordb
