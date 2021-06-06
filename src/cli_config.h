#ifndef __VECTORDB_CLI_CONFIG_H__
#define __VECTORDB_CLI_CONFIG_H__

#include "status.h"

namespace vectordb {

class CliConfig {
  public:
    static CliConfig&
    GetInstance() {
        static CliConfig instance;
        return instance;
    }

    Status Load(int argc, char **argv);

    CliConfig(const CliConfig&) = delete;
    CliConfig& operator=(const CliConfig&) = delete;

    std::string address() const;

  private:
    CliConfig() = default;
    ~CliConfig() = default;
    void ParseHostPort(const std::string &hp, std::string &host, int &port);

    std::string host_;
    int port_;
};

}  // namespace vectordb

#endif
