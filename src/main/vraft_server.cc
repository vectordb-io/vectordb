#include <csignal>
#include <iostream>

#include "coding.h"
#include "config.h"
#include "logger.h"
#include "raft_server.h"
#include "vraft_logger.h"

vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("vraft");
vraft::RaftServerSPtr server;

void SignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << ", quit ..." << std::endl;
  {
    auto sptr = server;
    loop->RunFunctor([sptr] { sptr->Stop(); });
  }
  loop->Stop();
}

int main(int argc, char **argv) {
  if (argc == 1) {
    std::cout << vraft::GetConfig().UsageBanner(argv[0]) << std::endl;
    return 0;
  }

  vraft::GetConfig().Parse(argc, argv);
  if (vraft::GetConfig().result().count("h")) {
    std::cout << vraft::GetConfig().Usage() << std::endl;
    return 0;
  }

  vraft::LoggerOptions logger_options{"vraft", false, 1, 8192,
                                      vraft::kLoggerTrace};
  logger_options.level = vraft::U8ToLevel(vraft::GetConfig().log_level());
  logger_options.enable_debug = vraft::GetConfig().enable_debug();

  std::string log_file = vraft::GetConfig().path() + "/log/vraft.log";
  vraft::vraft_logger.Init(log_file, logger_options);

  std::signal(SIGINT, SignalHandler);
  vraft::CodingInit();

  if (vraft::GetConfig().mode() == vraft::kSingleMode) {
    server = std::make_shared<vraft::RaftServer>(loop, vraft::GetConfig());
    server->Start();

  } else if (vraft::GetConfig().mode() == vraft::kSingleMode) {
    assert(0);

  } else {
    assert(0);
  }

  {
    loop->Init();
    auto sptr = loop;
    std::thread t([sptr] { sptr->Loop(); });
    loop->WaitStarted();
    t.join();
  }

  server.reset();
  loop.reset();

  vraft::Logger::ShutDown();
  return 0;
}
