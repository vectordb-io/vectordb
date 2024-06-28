#include "config.h"

#include "util.h"

namespace vraft {

Config default_config;
Config &GetConfig() { return default_config; }

void Config::Parse(int32_t argc, char **argv) {
  options_ = std::make_shared<cxxopts::Options>(std::string(argv[0]),
                                                UsageBanner(argv[0]));
  options_->add_options()(
      "addr", "my address, host_name:port",
      cxxopts::value<std::string>()->default_value("127.0.0.1:9000"))(
      "peers", "peer address, host_name:port,host_name:port",
      cxxopts::value<std::string>()->default_value(
          "127.0.0.1:9001,127.0.0.1:9002"))("h,help", "display help message")(
      "path", "home path",
      cxxopts::value<std::string>()->default_value("/tmp/vraft"))(
      "mode", "single/multi",
      cxxopts::value<std::string>()->default_value("single"))(
      "loglevel", "trace/debug/info/warn/error/fatal/off",
      cxxopts::value<std::string>()->default_value("trace"))(
      "debug", "enable debug log");

  result_ = options_->parse(argc, argv);

  if (result_.count("addr")) {
    my_addr_ = HostPort(result_["addr"].as<std::string>());
  }

  if (result_.count("peers")) {
    std::string peers_str = result_["peers"].as<std::string>();
    std::vector<std::string> peers_vec;
    Split(peers_str, ',', peers_vec);
    for (auto &s : peers_vec) {
      peers_.push_back(HostPort(s));
    }
  }

  if (result_.count("path")) {
    path_ = result_["path"].as<std::string>();
  }

  if (result_.count("mode")) {
    std::string mode_str = result_["mode"].as<std::string>();
    ToLower(mode_str);

    if (mode_str == "single") {
      mode_ = kSingleMode;
    } else if (mode_str == "multi") {
      mode_ = kMultiMode;
    } else {
      assert(0);
    }
  }

  if (result_.count("loglevel")) {
    std::string loglevel = result_["loglevel"].as<std::string>();
    ToLower(loglevel);

    if (loglevel == "debug") {
      log_level_ = 0;
    } else if (loglevel == "trace") {
      log_level_ = 1;
    } else if (loglevel == "info") {
      log_level_ = 2;
    } else if (loglevel == "warn") {
      log_level_ = 3;
    } else if (loglevel == "error") {
      log_level_ = 4;
    } else if (loglevel == "fatal") {
      log_level_ = 5;
    } else if (loglevel == "off") {
      log_level_ = 6;
    } else {
      assert(0);
    }
  }

  if (result_.count("debug")) {
    enable_debug_ = true;
  } else {
    enable_debug_ = false;
  }
}

void GenerateRotateConfig(std::vector<vraft::Config> &configs) {
  std::vector<vraft::HostPort> hps;
  hps.push_back(vraft::GetConfig().my_addr());
  for (auto hp : vraft::GetConfig().peers()) {
    hps.push_back(hp);
  }
  for (size_t i = 0; i < hps.size(); ++i) {
    // use i for myself
    vraft::Config c = vraft::GetConfig();
    c.set_my_addr(hps[i]);
    c.peers().clear();
    for (size_t j = 0; j < hps.size(); ++j) {
      if (j != i) {
        c.peers().push_back(hps[j]);
      }
    }
    std::string path = c.path();
    c.set_path(path + "/" + c.my_addr().ToString());
    configs.push_back(c);
  }
}

}  // namespace vraft
