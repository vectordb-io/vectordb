#include <csignal>

#include "cli_config.h"
#include "common.h"
#include "local_console.h"
#include "vdb_common.h"
#include "vdb_console.h"
#include "vraft_logger.h"

vraft::ConsoleWPtr wptr;

void SignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << std::endl;
  auto sptr = wptr.lock();
  if (sptr) {
    sptr->Stop();
  }

  std::cout << "vectordb-console stop ..." << std::endl;
  vraft::Logger::ShutDown();
  exit(0);
}

int main(int argc, char **argv) {
  // std::signal(SIGINT, SignalHandler);
  vraft::CodingInit();
  vectordb::CliConfigSPtr config = vectordb::CliConfigSingleton::GetInstance();

  if (argc == 1) {
    std::cout << config->UsageBanner(argv[0]) << std::endl;
    return 0;
  }

  config->Parse(argc, argv);
  if (config->result().count("h")) {
    std::cout << config->Usage() << std::endl;
    return 0;
  }

  vraft::LoggerOptions logger_options{"vectordb-cli",      false, 1, 8192,
                                      vraft::kLoggerTrace, true};
  vraft::vraft_logger.Init("/tmp/vectordb-cli.log", logger_options);
  logger_options.enable_debug = true;

  vraft::ConsoleSPtr console;
  if (config->path() == "") {
    vraft::Console *c =
        new vectordb::VdbConsole("vectordb-cli", config->addr().ToString());
    console.reset(c);
  } else {
    vraft::Console *c =
        new vectordb::LocalConsole("vectordb-cli", config->path());
    console.reset(c);
  }

  console->Run();

  return 0;
}