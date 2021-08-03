#include <glog/logging.h>
#include "vengine.h"
#include "util.h"
#include "node.h"
#include "status.h"
#include "coding.h"
#include "vindex_knn_graph.h"

namespace vectordb {

VIndexKNNGraph::VIndexKNNGraph(const std::string &path, VEngine* vengine, KNNGraphParam *param)
    :path_(path),
     vengine_(vengine) {
    if (param) {
        k_ = param->k;
        all_keys_ = param->all_keys;
    } else {
        all_keys_ = nullptr;
    }
    assert(vengine_);
    db_knn_path_ = path_ + "/knn";
}

VIndexKNNGraph::VIndexKNNGraph(const std::string &path, VEngine* vengine) {

}

VIndexKNNGraph::~VIndexKNNGraph() {
    delete db_knn_;
    delete db_meta_;
}

Status
VIndexKNNGraph::GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) {
    /*
    std::string knn_key, knn_value;
    leveldb::Status ls;
    int count;
    results.clear();

    std::cout << "debug: limit: " << limit << std::endl;


    if (limit <= 0) {
    return Status::OK();
    }

    std::cout << "debug: k_: " << k_ << std::endl;

    count = 0;
    for (int i = 0; i < k_; ++i) {
    EncodeKey(key, i, knn_key);

    ls = db_knn_->Get(leveldb::ReadOptions(), knn_key, &knn_value);
    assert(ls.ok());

    std::string find_key;
    double distance;
    auto b = DecodeValue(knn_value, find_key, distance);
    assert(b);

    VecObj vo;
    std::string table_name;
    int partition_id;
    int replica_id;
    b = util::ParseReplicaName(vengine_->replica_name(), table_name, partition_id, replica_id);
    assert(b);
    auto s = Node::GetInstance().GetVec(table_name, find_key, vo);
    assert(s.ok());
    assert(find_key == vo.key());

    VecDtParam param;
    param.key = vo.key();
    param.distance = distance;
    param.attach_value1 = vo.attach_value1();
    param.attach_value2 = vo.attach_value2();
    param.attach_value3 = vo.attach_value3();

    VecDt vdt(param);
    results.push_back(vdt);

    std::cout << "debug getknn graph " << count;

    count++;
    if (count >= limit) {
        break;
    }
    }
    */
    return Status::OK();
}

Status
VIndexKNNGraph::GetKNN(const Vec &vec, int limit, std::vector<VecDt> &results) {
    return Status::OK();
}

Status
VIndexKNNGraph::Distance(const std::string &key1, const std::string &key2, double &distance) {
    Status s;
    VecObj vo1, vo2;
    s = vengine_->Get(key1, vo1);
    assert(s.ok());
    s = vengine_->Get(key2, vo2);
    assert(s.ok());

    auto b = util::Distance(vo1.vec().data(), vo2.vec().data(), distance);
    assert(b);
    return Status::OK();
}

Status
VIndexKNNGraph::Load() {
    leveldb::Options options;
    //options.create_if_missing = true;
    leveldb::Status status;

    status = leveldb::DB::Open(options, db_knn_path_, &db_knn_);
    assert(status.ok());

    auto s = LoadK();
    assert(s.ok());

    return Status::OK();
}

Status
VIndexKNNGraph::WriteK() {
    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;

    std::string value;
    Int322Str(k_, value);

    s = db_knn_->Put(write_options, KEY_KNN_GRAPH_K, value);
    assert(s.ok());
    return Status::OK();
}

Status
VIndexKNNGraph::LoadK() {
    bool b;
    std::string value;
    leveldb::Status s;

    s = db_knn_->Get(leveldb::ReadOptions(), KEY_KNN_GRAPH_K, &value);
    assert(s.ok());

    b = Str2Int32(value, k_);
    assert(b);

    return Status::OK();
}

Status
VIndexKNNGraph::Build() {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status;

    status = leveldb::DB::Open(options, db_knn_path_, &db_knn_);
    assert(status.ok());

    std::vector<VecDt> results;
    assert(all_keys_);
    std::vector<std::string> my_keys;
    vengine_->Keys(my_keys);

    /*
    for (auto &k : my_keys) {
        std::cout << "debug: -- " << k << std::endl;
    }
    */

    std::cout << "all_keys_.size():" << all_keys_->size() << std::endl;
    for (auto &k : *all_keys_) {
        std::cout << "debug: -- " << k << std::endl;
    }


    int debug = 0;
    LOG(INFO) << "debug: build knn begin";

    auto s = WriteK();
    assert(s.ok());

    for (auto &each_all_key : *all_keys_) {
        results.clear();
        for (auto &each_my_key : my_keys) {
            double distance;
            Status s;

            std::cout << debug++ << " " << each_all_key << " " << each_my_key << std::endl;
            //LOG(INFO) << "debug: build " <<  each_all_key << " " << each_my_key;

            VecObj vo_each_all_key, vo_each_my_key;

            std::string table_name;
            int partition_id;
            int replica_id;
            bool b = util::ParseReplicaName(vengine_->replica_name(), table_name, partition_id, replica_id);
            assert(b);
            s = Node::GetInstance().GetVec(table_name, each_all_key, vo_each_all_key);
            assert(s.ok());

            s = vengine_->Get(each_my_key, vo_each_my_key);
            assert(s.ok());

            b = util::Distance(vo_each_all_key.vec().data(), vo_each_my_key.vec().data(), distance);
            assert(b);

            VecDtParam param;
            param.key = vo_each_my_key.key();
            param.distance = distance;
            param.attach_value1 = vo_each_my_key.attach_value1();
            param.attach_value2 = vo_each_my_key.attach_value2();
            param.attach_value3 = vo_each_my_key.attach_value3();

            VecDt vdt(param);
            results.push_back(vdt);
        }
        std::sort(results.begin(), results.end());

        int count = 0;
        for (auto &vdt : results) {
            std::string knn_key, knn_value;
            EncodeKey(each_all_key, count, knn_key);
            EncodeValue(vdt.key(), vdt.distance(), knn_value);

            leveldb::Status s;
            leveldb::WriteOptions write_options;
            write_options.sync = true;
            s = db_knn_->Put(write_options, knn_key, knn_value);
            assert(s.ok());

            char buf[8];
            snprintf(buf, sizeof(buf), "%d", count);
            LOG(INFO) << "debug build wirte " << each_all_key << " " << std::string(buf) << " " << vdt.key();

            count++;
            if (count >= k_) {
                break;
            }
        }
    }

    LOG(INFO) << "debug: build knn end";

    return Status::OK();
}

void
VIndexKNNGraph::EncodeKey(const std::string &key, int sequence, std::string &s) const {
    vectordb_rpc::KNNKey pb;
    pb.set_key(key);
    pb.set_sequence(sequence);
    bool b = pb.SerializeToString(&s);
    assert(b);
}

bool
VIndexKNNGraph::DecodeKey(const std::string &s, std::string &key, int &sequence) const {
    vectordb_rpc::KNNKey pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        key = pb.key();
        sequence = pb.sequence();
    }
    return ret;
}

void
VIndexKNNGraph::EncodeValue(const std::string &key, double distance, std::string &s) const {
    vectordb_rpc::KNNValue pb;
    pb.set_key(key);
    pb.set_distance(distance);
    bool b = pb.SerializeToString(&s);
    assert(b);
}

bool
VIndexKNNGraph::DecodeValue(const std::string &s, std::string &key, double &distance) const {
    vectordb_rpc::KNNValue pb;
    bool ret = pb.ParseFromString(s);
    if (ret) {
        key = pb.key();
        distance= pb.distance();
    }
    return ret;
}


} // namespace vectordb
