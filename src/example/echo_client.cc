#include "echo_client.h"

#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#include "common.h"

vraft::TcpClientWPtr weak_client;
vraft::EventLoopWPtr weak_loop;

void SignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << std::endl;

  auto sptr_client = weak_client.lock();
  if (sptr_client) {
    sptr_client->Stop();
  }

  auto sptr_loop = weak_loop.lock();
  if (sptr_loop) {
    sptr_loop->Stop();
  }
}

int main(int argc, char **argv) {
  vraft::LoggerOptions logger_options{"echo-client",       false, 1, 8192,
                                      vraft::kLoggerTrace, true};
  vraft::vraft_logger.Init("/tmp/echo_client.log", logger_options);
  std::signal(SIGINT, SignalHandler);

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("echo-loop");
  int32_t rv = loop->Init();
  assert(rv == 0);

  vraft::TcpOptions opt = {true};
  vraft::HostPort dest_addr("127.0.0.1", 9000);
  weak_loop = loop;

  vraft::TcpClientSPtr client =
      std::make_shared<vraft::TcpClient>(loop, "echo-client", dest_addr, opt);
  client->set_on_connection_cb(std::bind(&OnConnection, std::placeholders::_1));
  client->set_on_message_cb(
      std::bind(&OnMessage, std::placeholders::_1, std::placeholders::_2));
  client->TimerConnect(100);
  weak_client = client;

  std::thread t([loop] { loop->Loop(); });
  t.join();

  std::cout << "echo-client stop ..." << std::endl;
  vraft::Logger::ShutDown();
  return 0;
}