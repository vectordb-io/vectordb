#include "echo_server.h"

#include <csignal>
#include <iostream>
#include <thread>

EchoServerWPtr weak_server;

void SignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << std::endl;
  auto sptr = weak_server.lock();
  if (sptr) {
    sptr->Stop();
  }
}

int main(int argc, char **argv) {
  vraft::LoggerOptions logger_options{"echo-server",       false, 1, 8192,
                                      vraft::kLoggerTrace, true};
  vraft::vraft_logger.Init("/tmp/echo_server.log", logger_options);
  std::signal(SIGINT, SignalHandler);

  vraft::TcpOptions opt = {true};
  vraft::HostPort listen_addr("127.0.0.1", 9000);

  int32_t server_num = 4;
  int32_t thread_num = 3;
  EchoServerSPtr server =
      std::make_shared<EchoServer>(listen_addr, opt, server_num, thread_num);
  weak_server = server;

  for (int32_t i = 0; i < thread_num; ++i) {
    for (int32_t j = 0; j < server_num; ++j) {
      vraft::HostPort addr(listen_addr.host, listen_addr.port++);
      std::cout << "echo-server listening on " << addr.ToString() << " ... "
                << std::endl;
    }
  }

  server->Start();
  server->Join();

  std::cout << "echo-server stop ..." << std::endl;
  vraft::Logger::ShutDown();
  return 0;
}