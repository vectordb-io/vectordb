#include <glog/logging.h>
#include "coding.h"

namespace vectordb {

namespace coding {

void
Pb2Replica(const vectordb_rpc::Replica &pb, Replica &replica) {
    replica.set_id(pb.id());
    replica.set_name(pb.name());
    replica.set_table_name(pb.table_name());
    replica.set_partition_name(pb.partition_name());
    replica.set_path(pb.path());
    replica.set_address(pb.address());
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
    table.set_name(pb.name());
    table.set_partition_num(pb.partition_num());
    table.set_replica_num(pb.replica_num());
    table.set_path(pb.path());
    table.set_dim(pb.dim());

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
        table.mutable_indices().insert(pb.indices(i));
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
    pb.set_path(table.path());
    pb.set_dim(table.dim());
    for (auto &p : table.partitions()) {
        vectordb_rpc::Partition* partition = pb.add_partitions();
        Partition2Pb(*(p.second), *partition);
    }

    for (auto &index_name: table.indices()) {
        pb.add_indices(index_name);
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

void
VecObj2Pb(const VecObj &vo, vectordb_rpc::VecObj &pb) {
    pb.set_key(vo.key());
    Vec2Pb(vo.vec(), *(pb.mutable_vec()));
    pb.set_attach_value1(vo.attach_value1());
    pb.set_attach_value2(vo.attach_value2());
    pb.set_attach_value3(vo.attach_value3());
}

void
Pb2VecObj(const vectordb_rpc::VecObj &pb, VecObj &vo) {
    vo.set_key(pb.key());
    Pb2Vec(pb.vec(), vo.mutable_vec());
    vo.set_attach_value1(pb.attach_value1());
    vo.set_attach_value2(pb.attach_value2());
    vo.set_attach_value3(pb.attach_value3());
}

void
VecObj2Str(const VecObj &vo, std::string &s) {
    vectordb_rpc::VecObj pb;
    VecObj2Pb(vo, pb);
    bool ret = pb.SerializeToString(&s);
    assert(ret);
}

bool
Str2VecObj(const std::string &s, VecObj &vo) {
    vectordb_rpc::VecObj pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        Pb2VecObj(pb, vo);
    }
    return ret;
}

void
Int322Str(int32_t i, std::string &s) {
    vectordb_rpc::Int32 pb;
    pb.set_data(i);
    bool ret = pb.SerializeToString(&s);
    assert(ret);
}

bool
Str2Int32(const std::string &s, int32_t &i) {
    vectordb_rpc::Int32 pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        i = pb.data();
    }
    return ret;
}

} // namespace coding

} // namespace vectordb
