#include <glog/logging.h>
#include "util.h"
#include "coding.h"
#include "vengine.h"
#include "vindex_annoy.h"

namespace vectordb {

VEngine::VEngine(std::string path, int dim,
                 const std::map<std::string, std::string> &indices)
    :path_(path), dim_(dim), indices_name_type_(indices) {
    data_path_ = path_ + "/data";
    index_path_ = path_ + "/index";
}

VEngine::~VEngine() {
    delete data_;
}

Status
VEngine::Init() {
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
VEngine::Build() {
    Status s;
    s = BuildData();
    assert(s.ok());
    s = BuildIndex();
    assert(s.ok());
    return Status::OK();
}

Status
VEngine::BuildData() {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, data_path_, &data_);
    assert(status.ok());
    return Status::OK();
}

Status
VEngine::BuildIndex() {
    util::Mkdir(index_path_);
    return Status::OK();
}

Status
VEngine::Load() {
    Status s;
    s = LoadData();
    assert(s.ok());
    s = LoadIndex();
    assert(s.ok());
    return Status::OK();
}

Status
VEngine::LoadData() {
    leveldb::Options options;
    //options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, data_path_, &data_);
    assert(status.ok());
    return Status::OK();
}

Status
VEngine::LoadIndex() {
    for (auto &kv : indices_name_type_) {
        std::string index_name = kv.first;
        std::string index_type = kv.second;

        auto s = AddIndex(index_name, index_type);
        if (!s.ok()) {
            ;
        }
    }
    return Status::OK();
}

Status
VEngine::Put(const std::string &key, const VecObj &vo) {
    if (vo.vec().dim() != dim_) {
        return Status::Corruption("dim error");
    }

    std::string value;
    VecObj2Str(vo, value);

    leveldb::Status s;
    leveldb::WriteOptions write_options;
    write_options.sync = true;
    s = data_->Put(write_options, key, value);
    assert(s.ok());

    return Status::OK();
}

Status
VEngine::Get(const std::string &key, VecObj &vo) const {
    bool b;
    leveldb::Status s;
    std::string value;
    s = data_->Get(leveldb::ReadOptions(), key, &value);
    if (s.IsNotFound()) {
        std::string msg = key;
        msg.append(" not found");
        return Status::NotFound(msg);
    }
    assert(s.ok());
    b = Str2VecObj(value, vo);

    LOG(INFO) << "debug: " << vo.attach_value1() << "++" << vo.attach_value2() << "++" << vo.attach_value3();

    assert(b);
    return Status::OK();
}

Status
VEngine::Delete(const std::string &key) {
    return Status::OK();
}

Status
VEngine::AddIndex(std::string index_name, std::string index_type) {
    std::string index_path = index_path_ + "/" + index_name;
    std::shared_ptr<VIndex> index_sp;
    if (index_type == VECTOR_INDEX_ANNOY) {
        index_sp = std::make_shared<VIndexAnnoy>(index_path, this);
        assert(index_sp);
        auto s = index_sp->Init();
        assert(s.ok());
    } else if (index_type == VECTOR_INDEX_KNNGRAPH) {

    } else {
        LOG(INFO) << "known index type:" << index_type;
    }

    if (index_sp) {
        indices_.insert(std::pair<std::string, std::shared_ptr<VIndex>>(index_name, index_sp));
        LOG(INFO) << "add index: " << index_name << " " << index_type;
    }
    return Status::OK();
}

bool
VEngine::HasIndex() const {
    return indices_.size() > 0;
}

Status
VEngine::GetKNN(const std::string &key, int limit, std::vector<VecDt> &results, const std::string &index_name) {
    auto it = indices_.find(index_name);
    if (it == indices_.end()) {
        LOG(INFO) << "index " << index_name << " not exist";
        return Status::Corruption("index not exist");
    }
    auto index_sp = it->second;
    assert(index_sp);

    auto s = index_sp->GetKNN(key, limit, results);
    assert(s.ok());
    return Status::OK();
}

Status
VEngine::GetKNN(const Vec &vec, int limit, std::vector<VecDt> &results, const std::string &index_name) {
    auto it = indices_.find(index_name);
    if (it == indices_.end()) {
        LOG(INFO) << "index " << index_name << " not exist";
        return Status::Corruption("index not exist");
    }
    auto index_sp = it->second;
    assert(index_sp);

    auto s = index_sp->GetKNN(vec, limit, results);
    assert(s.ok());
    return Status::OK();
}

} // namespace vectordb
