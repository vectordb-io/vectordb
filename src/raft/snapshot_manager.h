#ifndef VRAFT_SNAPSHOT_MANAGER_H_
#define VRAFT_SNAPSHOT_MANAGER_H_

#include <unordered_map>
#include <vector>

#include "common.h"
#include "nlohmann/json.hpp"
#include "raft_addr.h"
#include "snapshot.h"

namespace vraft {

class SnapshotManager final {
 public:
  explicit SnapshotManager(const std::vector<RaftAddr> &peers);
  ~SnapshotManager();
  SnapshotManager(const SnapshotManager &) = delete;
  SnapshotManager &operator=(const SnapshotManager &) = delete;

  void Reset(const std::vector<RaftAddr> &peers);

 public:
  std::unordered_map<uint64_t, Snapshot> snapshots;
};

inline SnapshotManager::SnapshotManager(const std::vector<RaftAddr> &peers) {
  Reset(peers);
}

inline SnapshotManager::~SnapshotManager() {}

inline void SnapshotManager::Reset(const std::vector<RaftAddr> &peers) {
  snapshots.clear();
  for (auto addr : peers) {
    Snapshot snapshot;
    snapshot.reader_ = nullptr;
    snapshot.writer_ = nullptr;
    snapshots[addr.ToU64()] = snapshot;
  }
}

}  // namespace vraft

#endif
