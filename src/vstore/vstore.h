#ifndef VSTORE_VSTORE_H_
#define VSTORE_VSTORE_H_

#include <memory>

#include "buffer.h"
#include "common.h"
#include "raft.h"
#include "vstore_common.h"

namespace vstore {

class Vstore final {
 public:
  explicit Vstore(vraft::EventLoopSPtr &loop, vraft::Config &config);
  ~Vstore();
  Vstore(const Vstore &) = delete;
  Vstore &operator=(const Vstore &) = delete;

  void Start();
  void Stop();

 private:
  vraft::RaftServerSPtr raft_server_;
  vraft::EventLoopWPtr loop_;
};

inline Vstore::~Vstore() {}

}  // namespace vstore

#endif
