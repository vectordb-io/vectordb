#include <csignal>
#include <iostream>

#include "coding.h"
#include "raft_server.h"
#include "vraft_logger.h"
#include "vstore.h"
#include "vstore_common.h"

vraft::EventLoopWPtr loop_wptr;
vstore::VstoreWPtr vstore_wptr;

void SignalHandler(int signal) {
  std::cout << "recv signal " << strsignal(signal) << ", quit ..." << std::endl;
  auto loop = loop_wptr.lock();
  if (loop) {
    auto vstore = vstore_wptr.lock();
    if (vstore) {
      loop->RunFunctor([vstore] { vstore->Stop(); });
    }
    loop->Stop();
  }
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

  vraft::LoggerOptions logger_options{"vstore", false, 1, 8192,
                                      vraft::kLoggerTrace};
  logger_options.level = vraft::U8ToLevel(vraft::GetConfig().log_level());
  logger_options.enable_debug = vraft::GetConfig().enable_debug();

  std::string log_file = vraft::GetConfig().path() + "/log/vstore.log";
  vraft::vraft_logger.Init(log_file, logger_options);

  std::signal(SIGINT, SignalHandler);
  vraft::CodingInit();

  vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("vstore");
  loop->Init();
  loop_wptr = loop;

  vstore::VstoreSPtr vst =
      std::make_shared<vstore::Vstore>(loop, vraft::GetConfig());
  vstore_wptr = vst;
  vst->Start();

  std::thread t([loop] { loop->Loop(); });
  loop->WaitStarted();
  t.join();

  std::cout << "vstore-server stop ..." << std::endl;
  vraft::Logger::ShutDown();
  return 0;
}