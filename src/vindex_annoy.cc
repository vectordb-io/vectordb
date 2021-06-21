#include <glog/logging.h>
#include "vengine.h"
#include "util.h"
#include "coding.h"
#include "vindex_annoy.h"

namespace vectordb {

VIndexAnnoy::VIndexAnnoy(const std::string &path, VEngine* vengine)
    :path_(path),
     vengine_(vengine),
     annoy_index_(vengine->dim()) {
    assert(vengine);
    db_key2id_path_ = path_ + "/key2id";
    db_id2key_path_ = path_ + "/id2key";
    annoy_path_ = path_ + "/annoy.idx";
}

VIndexAnnoy::~VIndexAnnoy() {
    delete db_key2id_;
    delete db_id2key_;
}

Status
VIndexAnnoy::GetKNN(const std::string &key, int limit, std::vector<VecDt> &results) {
    int id;
    bool b;
    int search_k = 20;

    if (limit > 0) {
        b = Key2Id(key, id);
        assert(b);

        std::vector<int> result;
        std::vector<double> distances;
        annoy_index_.get_nns_by_item(id, limit, search_k, &result, &distances);
        assert(result.size() == distances.size());

        for (size_t i = 0; i < result.size(); ++i) {
            std::string find_key;
            b = Id2Key(result[i], find_key);
            assert(b);

            VecObj vo;
            auto s = vengine_->Get(find_key, vo);
            assert(s.ok());
            assert(find_key == vo.key());

            VecDtParam param;
            param.key = vo.key();
            param.distance = distances[i];
            param.attach_value1 = vo.attach_value1();
            param.attach_value2 = vo.attach_value2();
            param.attach_value3 = vo.attach_value3();

            VecDt vdt(param);
            results.push_back(vdt);

            LOG(INFO) << "debug: " << "id:" << result[i] << " " << find_key << distances[i];
        }
        std::sort(results.begin(), results.end());
    }
    return Status::OK();
}

Status
VIndexAnnoy::GetKNN(const Vec &vec, int limit, std::vector<VecDt> &results) {
    bool b;
    int search_k = 20;

    if (limit > 0) {
        std::vector<int> result;
        std::vector<double> distances;

        annoy_index_.get_nns_by_vector(vec.data().data(), vec.data().size(), search_k, &result, &distances);
        assert(result.size() == distances.size());

        for (size_t i = 0; i < result.size(); ++i) {
            std::string find_key;
            b = Id2Key(result[i], find_key);
            assert(b);

            VecObj vo;
            auto s = vengine_->Get(find_key, vo);
            assert(s.ok());
            assert(find_key == vo.key());

            VecDtParam param;
            param.key = vo.key();
            param.distance = distances[i];
            param.attach_value1 = vo.attach_value1();
            param.attach_value2 = vo.attach_value2();
            param.attach_value3 = vo.attach_value3();

            VecDt vdt(param);
            results.push_back(vdt);

            LOG(INFO) << "debug: getknn one " << vo.key() << " " << distances[i] << " " << vo.attach_value1() << "|" << vdt.attach_value1();
        }
        std::sort(results.begin(), results.end());
    }
    return Status::OK();
}

bool
VIndexAnnoy::Key2Id(const std::string &key, int &id) const {
    bool b;
    std::string value;
    leveldb::Status s;

    s = db_key2id_->Get(leveldb::ReadOptions(), key, &value);
    if (!s.ok()) {
        //LOG(INFO) << s.ToString();
    }
    assert(s.ok());
    b = Str2Int32(value, id);
    assert(b);
    return true;
}

bool
VIndexAnnoy::Id2Key(int id, std::string &key) const {
    std::string id_string;
    leveldb::Status s;

    Int322Str(id, id_string);
    s = db_id2key_->Get(leveldb::ReadOptions(), id_string, &key);
    assert(s.ok());
    return true;
}

Status
VIndexAnnoy::Distance(const std::string &key1, const std::string &key2, double &distance) {
    int id1, id2;
    bool b;
    std::string value;
    leveldb::Status s;

    b = Key2Id(key1, id1);
    assert(b);
    b = Key2Id(key2, id2);
    assert(b);

    distance = annoy_index_.get_distance(id1, id2);
    return Status::OK();
}

Status
VIndexAnnoy::Init() {
    Status s;
    if (util::DirOK(path_)) {
        s = Load();
        assert(s.ok());

    } else {
        util::Mkdir(path_);
        s = Build();
        assert(s.ok());
    }
    return Status::OK();
}

Status
VIndexAnnoy::Load() {
    leveldb::Options options;
    //options.create_if_missing = true;
    leveldb::Status status;

    status = leveldb::DB::Open(options, db_key2id_path_, &db_key2id_);
    assert(status.ok());
    status = leveldb::DB::Open(options, db_id2key_path_, &db_id2key_);
    assert(status.ok());

    auto b = annoy_index_.load(annoy_path_.c_str());
    assert(b);

    return Status::OK();
}

Status
VIndexAnnoy::Build() {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status;

    status = leveldb::DB::Open(options, db_key2id_path_, &db_key2id_);
    assert(status.ok());
    status = leveldb::DB::Open(options, db_id2key_path_, &db_id2key_);
    assert(status.ok());

    int annoy_index_id = 0;
    leveldb::Iterator* it = vengine_->data()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        std::string value = it->value().ToString();

        VecObj vo;
        bool b = Str2VecObj(value, vo);
        assert(b);

        double arr[1024];
        assert(vo.vec().dim() < 1024);
        for (int j = 0; j < vo.vec().dim(); ++j) {
            arr[j] = vo.vec().data()[j];
        }
        annoy_index_.add_item(annoy_index_id, reinterpret_cast<const double*>(arr));

        std::string id_string;
        Int322Str(annoy_index_id, id_string);

        leveldb::Status s;
        leveldb::WriteOptions write_options;
        write_options.sync = true;

        s = db_id2key_->Put(write_options, id_string, key);
        assert(s.ok());
        //LOG(INFO) << "build index id2key: " << annoy_index_id << " " << key;

        s = db_key2id_->Put(write_options, key, id_string);
        assert(s.ok());
        //LOG(INFO) << "build index key2id: " << key << " " << annoy_index_id;

        annoy_index_id++;
        if (annoy_index_id % 500 == 0) {
            printf("build index: annoy_index_id: %d\n", annoy_index_id);
            fflush(nullptr);
        }
    }
    assert(it->status().ok());  // Check for any errors found during the scan
    delete it;

    //annoy_index_.build(2 * dim());
    annoy_index_.build(10);
    LOG(INFO) << "build tree finish";

    annoy_index_.save(annoy_path_.c_str());
    LOG(INFO) << "save tree finish";

    return Status::OK();
}

} // namespace vectordb
