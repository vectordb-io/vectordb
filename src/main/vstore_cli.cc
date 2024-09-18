#include <csignal>
#include <iostream>

#include "coding.h"
#include "vraft_logger.h"
#include "vstore.h"
#include "vstore_common.h"
#include "vstore_console.h"

void SignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << ", quit ..." << std::endl;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cout << argv[0] << " 127.0.0.1:9000" << std::endl;
    exit(-1);
  }

  // std::signal(SIGINT, SignalHandler);
  vraft::CodingInit();

  vraft::LoggerOptions logger_options{
      "vstore", false, 1, 8192, vraft::kLoggerTrace, true};
  std::string log_file = "/tmp/vstore-cli.log";
  vraft::vraft_logger.Init(log_file, logger_options);

  vraft::ConsoleSPtr console(
      new vstore::VstoreConsole("vstore-cli", std::string(argv[1])));
  assert(console);
  console->Run();

  vraft::Logger::ShutDown();
  return 0;
}