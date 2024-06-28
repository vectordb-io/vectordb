#include <csignal>
#include <iostream>
#include <vector>

#include "coding.h"
#include "config.h"
#include "logger.h"
#include "raft_server.h"
#include "remu.h"
#include "test_suite.h"
#include "timer.h"
#include "vraft_logger.h"

void SignalHandler(int signal) {
  vraft::Logger::ShutDown();
  exit(signal);
}

vraft::Remu *r = nullptr;

void RemuTick(vraft::Timer *timer) {
  switch (vraft::current_state) {
    case vraft::kTestState0: {
      r->Print();
      int32_t leader_num = 0;
      for (auto ptr : r->raft_servers) {
        if (ptr->raft()->state() == vraft::LEADER) {
          leader_num++;
        }
      }

      if (leader_num == 1) {
        vraft::current_state = vraft::kTestStateEnd;
      }

      break;
    }
    case vraft::kTestStateEnd: {
      std::cout << "exit ..." << std::endl;
      exit(0);
    }
    default:
      break;
  }
}

int main(int argc, char **argv) {
  try {
    if (argc == 1) {
      std::cout << vraft::GetConfig().UsageBanner(argv[0]) << std::endl;
      return 0;
    }

    vraft::GetConfig().Parse(argc, argv);
    if (vraft::GetConfig().result().count("h")) {
      std::cout << vraft::GetConfig().Usage() << std::endl;
      return 0;
    }

    vraft::LoggerOptions logger_options{"remu", false, 1, 8192,
                                        vraft::kLoggerTrace};
    logger_options.level = vraft::U8ToLevel(vraft::GetConfig().log_level());
    logger_options.enable_debug = vraft::GetConfig().enable_debug();

    std::string log_file = vraft::GetConfig().path() + "/log/remu.log";
    vraft::vraft_logger.Init(log_file, logger_options);

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGSEGV, SignalHandler);
    vraft::CodingInit();

    vraft::EventLoopSPtr loop = std::make_shared<vraft::EventLoop>("remu");
    vraft::Remu remu(loop);
    r = &remu;

    vraft::TimerParam param;
    param.timeout_ms = 1000;
    param.repeat_ms = 1000;
    param.cb = RemuTick;
    param.data = nullptr;
    loop->AddTimer(param);
    GenerateRotateConfig(remu.configs);
    remu.Create();
    remu.Start();
    loop->Loop();

  } catch (const char *msg) {
    std::cerr << "execption caught: " << msg << std::endl;
  }

  return 0;
}
