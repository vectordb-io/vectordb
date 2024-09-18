#ifndef VECTORDB_VDB_CONFIG_H_
#define VECTORDB_VDB_CONFIG_H_

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "cxxopts.hpp"
#include "hostport.h"
#include "vdb_common.h"

namespace vectordb {

class VdbConfig final {
 public:
  explicit VdbConfig();
  ~VdbConfig();
  VdbConfig(const VdbConfig &) = delete;
  VdbConfig &operator=(const VdbConfig &) = delete;

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

inline VdbConfig::VdbConfig() {}

inline VdbConfig::~VdbConfig() {}

class ConfigSingleton {
 public:
  ConfigSingleton(const ConfigSingleton &) = delete;
  ConfigSingleton &operator=(const ConfigSingleton &) = delete;

  static VdbConfigSPtr GetInstance() {
    static std::once_flag init_flag;
    std::call_once(init_flag, []() { instance_.reset(new VdbConfig); });

    VdbConfigSPtr sptr;
    {
      std::unique_lock<std::mutex> ulk(mu_);
      sptr = instance_;
    }
    return sptr;
  }

 private:
  ConfigSingleton() {}
  ~ConfigSingleton() {}

  static std::mutex mu_;
  static VdbConfigSPtr instance_;
};

}  // namespace vectordb

#endif
