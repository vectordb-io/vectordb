#ifndef VECTORDB_VINDEX_H
#define VECTORDB_VINDEX_H

#include <string>

#include "common.h"
#include "hnswlib/hnswlib.h"
#include "retno.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/write_batch.h"
#include "vdb.pb.h"

namespace vectordb {

class VIndex {
 public:
  VIndex(const vdb::IndexParam &param);
  ~VIndex();

  VIndex(const VIndex &) = delete;
  VIndex &operator=(const VIndex &) = delete;

  RetNo Add(int64_t id, const std::vector<float> &vector);
  RetNo Persist();

  // input: v, k
  // output: ids, distances, scalars
  RetNo Search(const std::vector<float> &vector, int32_t k,
               std::vector<int64_t> &ids, std::vector<float> &distances);

  // input: id, k
  // output: ids, distances, scalars
  RetNo Search(int64_t id, int32_t k, std::vector<int64_t> &ids,
               std::vector<float> &distances);

  int32_t Size() const;

  const vdb::IndexParam &param() const { return param_; }
  RetNo GetVecByID(int64_t id, std::vector<float> &vector);

 private:
  void Init();
  RetNo New();
  RetNo Load();
  void Prepare();
  json ToJson() const;
  void PersistDescription();
  void PersistIndex();
  RetNo NewIndex();
  RetNo LoadIndex();

 private:
  std::string data_path_;
  std::string description_file_;
  vdb::IndexParam param_;

  // hnswlib index
  std::string hindex_file_;
  std::unique_ptr<hnswlib::AlgorithmInterface<float>> hindex_;
  std::shared_ptr<hnswlib::SpaceInterface<float>> hspace_;
};

using VIndexSPtr = std::shared_ptr<VIndex>;

}  // namespace vectordb

#endif  // VECTORDB_VINDEX_H