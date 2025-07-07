#ifndef VECTORDB_TABLE_H
#define VECTORDB_TABLE_H

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "common.h"
#include "options.h"
#include "retno.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/write_batch.h"
#include "vdb.pb.h"
#include "vindex.h"

namespace vectordb {

extern const IndexType kDefaultIndexType;
extern const DistanceType kDefaultDistanceType;

class Table final {
 public:
  Table(const vdb::TableParam &param);
  ~Table();

  Table(const Table &) = delete;
  Table &operator=(const Table &) = delete;

  RetNo Add(int64_t id, std::vector<float> &vector, const std::string &scalar,
            const WOptions &options = WOptions(), bool normalize = false);

  RetNo Add(int64_t id, std::vector<float> &vector,
            const WOptions &options = WOptions(), bool normalize = false);

  // input: id
  // output: vector, scalar
  RetNo Get(int64_t id, std::vector<float> &vector, std::string &scalar);

  // input: id
  // output: vector
  RetNo Get(int64_t id, std::vector<float> &vector);

  // input: id
  // output: scalar
  RetNo Get(int64_t id, std::string &scalar);

  // input: v, k
  // output: ids, distances, scalars
  // index_id: -1 means the newest index
  RetNo Search(const std::vector<float> &v, int32_t k,
               std::vector<int64_t> &ids, std::vector<float> &distances,
               std::vector<std::string> &scalars,
               const ROptions &options = ROptions(), int32_t index_id = -1);

  // input: id, k
  // output: ids, distances, scalars
  // index_id: -1 means the newest index
  RetNo Search(int64_t id, int32_t k, std::vector<int64_t> &ids,
               std::vector<float> &distances, std::vector<std::string> &scalars,
               const ROptions &options = ROptions(), int32_t index_id = -1);

  RetNo BuildIndex();
  RetNo BuildIndex(const vdb::FlatParam &param);
  RetNo BuildIndex(const vdb::HnswParam &param);

  // the newest 'left' indexes will be kept
  RetNo DropIndex(int32_t left = 2);

  const vdb::TableParam &param() const { return param_; }

  RetNo Persist();
  void PersistDescription();
  void PersistIndex();

  std::vector<int32_t> IndexIDs() const;

 private:
  void Init();
  RetNo New();
  RetNo Load();
  void Prepare();
  RetNo LoadIndex();
  RetNo NewData();
  RetNo LoadData();
  int32_t MaxIndexID() const;
  json ToJson() const;
  RetNo DoBuildIndex(const vdb::IndexParam &param);

  // input: v, k
  // output: ids, distances, scalars
  RetNo DoSearch(VIndexSPtr index, const std::vector<float> &v, int32_t k,
                 std::vector<int64_t> &ids, std::vector<float> &distances,
                 std::vector<std::string> &scalars,
                 const ROptions &options = ROptions());

 private:
  std::string data_path_;
  std::string index_path_;
  std::string description_file_;
  vdb::TableParam param_;

  std::shared_ptr<rocksdb::DB> data_;
  std::unordered_map<int32_t, VIndexSPtr> indexes_;
  std::unordered_map<std::string, rocksdb::ColumnFamilyHandle *> cf_handles_;
};

vdb::FlatParam DefaultFlatParam(int32_t dim);
vdb::HnswParam DefaultHnswParam(int32_t dim);

using TableSPtr = std::shared_ptr<Table>;

}  // namespace vectordb

#endif