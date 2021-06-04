#ifndef __VECTORDB_CONFIG_H__
#define __VECTORDB_CONFIG_H__

#include <getopt.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include "status.h"

namespace vectordb {

class HostAndPort {
  public:
    HostAndPort() = default;
    HostAndPort(const HostAndPort&) = default;

    HostAndPort(std::string host, int port)
        :host_(host), port_(port) {
    }

    std::string
    ToString() const {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s:%d", host_.c_str(), port_);
        return std::string(buf);
    }

    void set_host(const std::string &host) {
        host_ = host;
    }

    void set_port(int port) {
        port_ = port;
    }

    std::string host() const {
        return host_;
    }

    int port() const {
        return port_;
    }

  private:
    std::string host_;
    int port_;
};

class Config {
  public:
    static Config&
    GetInstance() {
        static Config instance;
        return instance;
    }

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    std::string ToString() const;
    Status Load(int argc, char **argv);

    const HostAndPort& address() const {
        return address_;
    }

    const std::string& data_path() const {
        return data_path_;
    }

  private:
    Config();
    ~Config();

    void ParseHostPort(const std::string &hp, std::string &host, int &port);

    HostAndPort address_;
    std::string data_path_;
};

} // namespace vectordb

#endif
