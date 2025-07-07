#ifndef VECTORDB_VECTORDB_H
#define VECTORDB_VECTORDB_H

#include "vdb.h"

namespace vectordb {

class Vectordb {
 public:
  Vectordb(const std::string &name, const std::string &path);
  ~Vectordb();

  Vectordb(const Vectordb &) = delete;
  Vectordb &operator=(const Vectordb &) = delete;

  std::string Version() const { return vdb_->Version(); }

  RetNo CreateTable(const std::string &name, int32_t dim);
  RetNo CreateTable(const std::string &name,
                    const vdb::IndexInfo &default_index_info);
  RetNo DropTable(const std::string &name, bool delete_data = false);

  RetNo Add(const std::string &table_name, int64_t id,
            std::vector<float> &vector, const std::string &scalar,
            const WOptions &options = WOptions(), bool normalize = false);

  RetNo Add(const std::string &table_name, int64_t id,
            std::vector<float> &vector, const WOptions &options = WOptions(),
            bool normalize = false);

  // input: id
  // output: vector, scalar
  RetNo Get(const std::string &table_name, int64_t id,
            std::vector<float> &vector, std::string &scalar);

  // input: id
  // output: vector
  RetNo Get(const std::string &table_name, int64_t id,
            std::vector<float> &vector);

  // input: id
  // output: scalar
  RetNo Get(const std::string &table_name, int64_t id, std::string &scalar);

  // input: v, k
  // output: ids, distances, scalars
  // index_id: -1 means the newest index
  RetNo Search(const std::string &table_name, const std::vector<float> &v,
               int32_t k, std::vector<int64_t> &ids,
               std::vector<float> &distances, std::vector<std::string> &scalars,
               const ROptions &options = ROptions(), int32_t index_id = -1);

  // input: id, k
  // output: ids, distances, scalars
  // index_id: -1 means the newest index
  RetNo Search(const std::string &table_name, int64_t id, int32_t k,
               std::vector<int64_t> &ids, std::vector<float> &distances,
               std::vector<std::string> &scalars,
               const ROptions &options = ROptions(), int32_t index_id = -1);

  RetNo BuildIndex(const std::string &table_name);

  RetNo BuildIndex(const std::string &table_name, const vdb::IndexInfo &param);

  // the newest 'left' indexes will be kept
  RetNo DropIndex(const std::string &table_name, int32_t left = 2);

  vdb::DBParam Meta();

  std::string MetaStr() const;

  std::vector<int32_t> IndexIDs(const std::string &table_name);

  RetNo Persist();
  RetNo Persist(const std::string &table_name);

 private:
  void Init();
  RetNo New();
  RetNo Load();
  void Prepare();

  RetNo PersistMeta();
  vdb::DBParam LoadMeta();

 private:
  std::string name_;
  std::string path_;
  std::string data_path_;
  std::string meta_path_;
  std::string log_path_;

  VdbSPtr vdb_;
  std::shared_ptr<rocksdb::DB> meta_;
};

}  // namespace vectordb

#endif  // VECTORDB_VECTORDB_H