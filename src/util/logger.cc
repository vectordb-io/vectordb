#include "logger.h"

namespace vectordb {

const std::string kLoggerName = "vdb";
std::shared_ptr<spdlog::logger> logger;

void InitLogger(const std::string &logfile) {
  if (!logger) {
    logger = spdlog::basic_logger_st(kLoggerName, logfile);
    logger->set_level(spdlog::level::debug);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%s:%#] %v");
  }
}

void DestroyLogger() {
  if (logger) {
    spdlog::drop(kLoggerName);
    logger.reset();
  }
}

void DisableLog() { logger->set_level(spdlog::level::off); }

void EnableLog() { logger->set_level(spdlog::level::debug); }

}  // namespace vectordb