#ifndef VSTORE_VSTORE_SM_H_
#define VSTORE_VSTORE_SM_H_

#include <memory>

#include "leveldb/db.h"
#include "nlohmann/json.hpp"
#include "state_machine.h"

namespace vstore {

vraft::StateMachineSPtr CreateVStoreSM(std::string &path);

class VstoreSm : public vraft::StateMachine {
 public:
  explicit VstoreSm(std::string path);
  ~VstoreSm();
  VstoreSm(const VstoreSm &) = delete;
  VstoreSm &operator=(const VstoreSm &) = delete;

  int32_t Restore() override;
  int32_t Apply(vraft::LogEntry *entry, vraft::RaftAddr addr) override;
  vraft::RaftIndex LastIndex() override;
  vraft::RaftTerm LastTerm() override;

  int32_t Get(const std::string &key, std::string &value);

 private:
  std::shared_ptr<leveldb::DB> db_;
};

inline VstoreSm::~VstoreSm() {}

}  // namespace vstore

#endif
