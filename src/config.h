#ifndef __VECTORDB_CONFIG_H__
#define __VECTORDB_CONFIG_H__

#include <getopt.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include "jsonxx/json.hpp"
#include "util.h"
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

    bool ParseFromString(const std::string &s) {
        std::vector<std::string> sv;
        util::Split(s, ':', sv, " \t");
        if (sv.size() != 2) {
            return false;
        }
        host_ = sv[0];
        sscanf(sv[1].c_str(), "%d", &port_);
        return true;
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

    const std::string meta_path() const {
        return data_path_ + "/meta";
    }

    const std::string engine_path() const {
        return data_path_ + "/data";
    }

  private:
    Config() {}
    ~Config() {}

    HostAndPort address_;
    std::string data_path_;
};

} // namespace vectordb

#endif
