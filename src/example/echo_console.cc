#include "echo_console.h"

#include <csignal>
#include <memory>

#include "console.h"
#include "vraft_logger.h"

using EchoConsoleSPtr = std::shared_ptr<EchoConsole>;
EchoConsoleSPtr console_ptr;

void SignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << std::endl;
  console_ptr->Stop();

  std::cout << "echo-console stop ..." << std::endl;
  vraft::Logger::ShutDown();
  exit(0);
}

int main(int argc, char **argv) {
  vraft::LoggerOptions logger_options{"echo-console",      false, 1, 8192,
                                      vraft::kLoggerTrace, true};
  vraft::vraft_logger.Init("/tmp/echo_console.log", logger_options);

  std::signal(SIGINT, SignalHandler);

  EchoConsoleSPtr console =
      std::make_shared<EchoConsole>("echo", "127.0.0.1:9000");
  console_ptr = console;
  console->Run();

  assert(0);
  return 0;
}
