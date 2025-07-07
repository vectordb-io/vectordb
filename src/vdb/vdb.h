#ifndef VECTORDB_VDB_H
#define VECTORDB_VDB_H

#include <spdlog/spdlog.h>

#include <experimental/filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"
#include "options.h"
#include "retno.h"
#include "table.h"

namespace vectordb {

class Vdb final {
 public:
  Vdb(const vdb::DBParam &param);
  ~Vdb();

  Vdb(const Vdb &) = delete;
  Vdb &operator=(const Vdb &) = delete;

  std::string Version() const;

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

  const vdb::DBParam &Meta() const { return param_; }

  std::string MetaStr() const;

  std::vector<int32_t> IndexIDs(const std::string &table_name);

  RetNo Persist();
  RetNo Persist(const std::string &table_name);

 private:
  void Init();
  RetNo New();
  RetNo Load();
  void Prepare();
  TableSPtr GetTable(const std::string &name);

 private:
  vdb::DBParam param_;
  std::unordered_map<std::string, TableSPtr> tables_;
};

using VdbSPtr = std::shared_ptr<Vdb>;
using VdbUPtr = std::unique_ptr<Vdb>;

}  // namespace vectordb

#endif