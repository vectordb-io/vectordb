#ifndef VECTORDB_VECTORDB_H_
#define VECTORDB_VECTORDB_H_

#include <atomic>
#include <memory>

#include "buffer.h"
#include "common.h"
#include "metadata.h"
#include "server_thread.h"
#include "unordered_map"
#include "vdb_config.h"
#include "vengine.h"

namespace vectordb {

class VectorDB {
 public:
  explicit VectorDB(VdbConfigSPtr config);
  ~VectorDB();
  VectorDB(const VectorDB &) = delete;
  VectorDB &operator=(const VectorDB &) = delete;

  void Start();
  void Join();
  void Stop();

  int32_t AddTable(TableParam param);

 private:
  void OnConnection(const vraft::TcpConnectionSPtr &conn);
  void OnMessage(const vraft::TcpConnectionSPtr &conn, vraft::Buffer *buf);
  void OnMsgVersion(const vraft::TcpConnectionSPtr &conn,
                    struct MsgVersion &msg);
  int32_t CreateVEngine(ReplicaSPtr replica);

 private:
  std::atomic<bool> start_;
  VdbConfigSPtr config_;
  std::string path_;
  std::string meta_path_;
  std::string data_path_;

  MetadataSPtr meta_;
  std::unordered_map<uint64_t, VEngineSPtr> engines_;

  vraft::ServerThreadSPtr server_thread_;
  vraft::WorkThreadPoolSPtr worker_pool_;

  std::atomic<uint64_t> seqid_;
  std::unordered_map<uint64_t, vraft::TcpConnectionSPtr>
      requests_;  // <seqid, connection>
};

inline VectorDB::~VectorDB() {}

}  // namespace vectordb

#endif
