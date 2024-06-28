#include <algorithm>
#include <cstdlib>
#include <fstream>

#include "clock.h"
#include "raft.h"
#include "raft_server.h"
#include "util.h"
#include "vraft_logger.h"

namespace vraft {

int32_t Raft::OnInstallSnapshot(struct InstallSnapshot &msg) {
  if (started_) {
    ;
  }
  return 0;
}

int32_t Raft::SendInstallSnapshotReply(InstallSnapshotReply &msg,
                                       Tracer *tracer) {
  if (started_) {
    ;
  }
  return 0;
}

int32_t Raft::OnInstallSnapshotReply(struct InstallSnapshotReply &msg) {
  if (started_) {
    ;
  }
  return 0;
}

}  // namespace vraft
