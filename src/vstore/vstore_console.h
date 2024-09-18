#ifndef VSTORE_VSTORE_CONSOLE_H_
#define VSTORE_VSTORE_CONSOLE_H_

#include <memory>

#include "console.h"

namespace vstore {

class VstoreConsole;
using VstoreConsoleSPtr = std::shared_ptr<VstoreConsole>;
using VstoreConsoleUPtr = std::unique_ptr<VstoreConsole>;
using VstoreConsoleWPtr = std::weak_ptr<VstoreConsole>;

class VstoreConsole : public vraft::Console {
 public:
  explicit VstoreConsole(const std::string &name, const std::string &addr);
  ~VstoreConsole();
  VstoreConsole(const VstoreConsole &) = delete;
  VstoreConsole &operator=(const VstoreConsole &) = delete;

 private:
  int32_t Parse(const std::string &cmd_line) override;
  int32_t Execute() override;
  void OnMessage(const vraft::TcpConnectionSPtr &conn,
                 vraft::Buffer *buf) override;
  std::string HelpString() override;

  void Clear();

 private:
  std::string cmd_;
  std::string key_;
  std::string value_;
  std::string leader_transfer_;
};

inline VstoreConsole::VstoreConsole(const std::string &name,
                                    const std::string &addr)
    : Console(name, vraft::HostPort(addr)) {}

inline VstoreConsole::~VstoreConsole() {}

}  // namespace vstore

#endif
