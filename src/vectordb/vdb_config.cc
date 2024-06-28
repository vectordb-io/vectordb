#include "vdb_config.h"

namespace vectordb {

void VdbConfig::Parse(int32_t argc, char **argv) {
  options_ = std::make_shared<cxxopts::Options>(std::string(argv[0]),
                                                UsageBanner(argv[0]));
  options_->add_options()(
      "addr", "my address, host_name:port",
      cxxopts::value<std::string>()->default_value("127.0.0.1:9000"))(
      "h,help", "display help message")(
      "path", "home path",
      cxxopts::value<std::string>()->default_value("/tmp/vectordb_dir"))(
      "loglevel", "trace/debug/info/warn/error/fatal/off",
      cxxopts::value<std::string>()->default_value("trace"))(
      "debug", "enable debug log");

  result_ = options_->parse(argc, argv);

  if (result_.count("addr")) {
    addr_ = vraft::HostPort(result_["addr"].as<std::string>());
  }

  if (result_.count("path")) {
    path_ = result_["path"].as<std::string>();
  }

  if (result_.count("loglevel")) {
    std::string loglevel = result_["loglevel"].as<std::string>();
    vraft::ToLower(loglevel);

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

const std::string VdbConfig::Usage() {
  std::string str;
  if (options_) {
    str = options_->help();
  }
  return str;
}

const std::string VdbConfig::UsageBanner(char *program_name) {
  std::string str = "Example:\n";
  char buf[256];
  snprintf(buf, sizeof(buf),
           "%s --addr=127.0.0.1:9000 --path=/tmp/vectordb_dir\n", program_name);
  str.append(buf);
  snprintf(buf, sizeof(buf),
           "%s --addr=127.0.0.1:9000 --path=/tmp/vectordb_dir --debug\n",
           program_name);
  str.append(buf);
  return str;
}

const std::string VdbConfig::ProgramName() { return options_->program(); }

const std::string VdbConfig::ToString() const {
  std::string str;
  char buf[512];
  snprintf(buf, sizeof(buf), "addr: %s\n", addr_.ToString().c_str());
  str.append(buf);
  snprintf(buf, sizeof(buf), "path: %s\n", path_.c_str());
  str.append(buf);

  return str;
}

std::mutex ConfigSingleton::mu_;
VdbConfigSPtr ConfigSingleton::instance_ = nullptr;

}  // namespace vectordb
