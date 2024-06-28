#include <csignal>
#include <iostream>

#include "coding.h"
#include "vdb_config.h"
#include "vectordb.h"

vectordb::VectorDBWPtr wptr;

void SignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << ", quit ..." << std::endl;
  auto sptr = wptr.lock();
  if (sptr) {
    sptr->Stop();
  }
}

int main(int argc, char **argv) {
  std::signal(SIGINT, SignalHandler);
  vraft::CodingInit();
  vectordb::VdbConfigSPtr config = vectordb::ConfigSingleton::GetInstance();

  if (argc == 1) {
    std::cout << config->UsageBanner(argv[0]) << std::endl;
    return 0;
  }

  config->Parse(argc, argv);
  if (config->result().count("h")) {
    std::cout << config->Usage() << std::endl;
    return 0;
  }

  vraft::LoggerOptions logger_options{"vectordb",          false, 1, 8192,
                                      vraft::kLoggerTrace, true};
  logger_options.level = vraft::U8ToLevel(config->log_level());
  logger_options.enable_debug = config->enable_debug();

  std::string log_file = config->path() + "/log/vectordb-server.log";
  vraft::vraft_logger.Init(log_file, logger_options);

  vectordb::VectorDBSPtr vdb = std::make_shared<vectordb::VectorDB>(config);
  wptr = vdb;
  vdb->Start();
  vdb->Join();

  std::cout << "vectordb-server stop ..." << std::endl;
  vraft::Logger::ShutDown();
  return 0;
}