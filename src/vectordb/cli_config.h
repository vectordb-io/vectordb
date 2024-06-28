#ifndef VECTORDB_CLI_CONFIG_H_
#define VECTORDB_CLI_CONFIG_H_

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "cxxopts.hpp"
#include "hostport.h"

namespace vectordb {

class CliConfig;
using CliConfigSPtr = std::shared_ptr<CliConfig>;
using CliConfigUPtr = std::unique_ptr<CliConfig>;
using CliConfigWPtr = std::weak_ptr<CliConfig>;

class CliConfig final {
 public:
  explicit CliConfig();
  ~CliConfig();
  CliConfig(const CliConfig &) = delete;
  CliConfig &operator=(const CliConfig &) = delete;

  void Parse(int32_t argc, char **argv);
  const std::string Usage();
  const std::string UsageBanner(char *program_name);
  const std::string ProgramName();
  const cxxopts::ParseResult &result() const { return result_; }
  const std::string ToString() const;

  vraft::HostPort addr() { return addr_; }
  uint8_t log_level() const { return log_level_; }
  bool enable_debug() const { return enable_debug_; }
  std::string path() { return path_; }

 private:
  std::shared_ptr<cxxopts::Options> options_;
  cxxopts::ParseResult result_;

  vraft::HostPort addr_;
  uint8_t log_level_;
  bool enable_debug_;
  std::string path_;
};

inline CliConfig::CliConfig() {}

inline CliConfig::~CliConfig() {}

class CliConfigSingleton {
 public:
  CliConfigSingleton(const CliConfigSingleton &) = delete;
  CliConfigSingleton &operator=(const CliConfigSingleton &) = delete;

  static CliConfigSPtr GetInstance() {
    static std::once_flag init_flag;
    std::call_once(init_flag, []() { instance_.reset(new CliConfig); });

    CliConfigSPtr sptr;
    {
      std::unique_lock<std::mutex> ulk(mu_);
      sptr = instance_;
    }
    return sptr;
  }

 private:
  CliConfigSingleton() {}
  ~CliConfigSingleton() {}

  static std::mutex mu_;
  static CliConfigSPtr instance_;
};

}  // namespace vectordb

#endif
