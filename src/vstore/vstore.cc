#include "vstore.h"

#include "raft_server.h"
#include "vstore_sm.h"

namespace vstore {

Vstore::Vstore(vraft::EventLoopSPtr &loop, vraft::Config &config)
    : loop_(loop) {
  raft_server_ = std::make_shared<vraft::RaftServer>(loop, config);
  raft_server_->raft()->set_create_sm(CreateVStoreSM);
  raft_server_->raft()->set_print_screen(config.print_screen());
}

void Vstore::Start() { raft_server_->Start(); }

void Vstore::Stop() { raft_server_->Stop(); }

}  // namespace vstore
