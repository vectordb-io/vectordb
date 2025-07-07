#ifndef VECTORDB_UTIL_LOGGER_H
#define VECTORDB_UTIL_LOGGER_H

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <memory>

namespace vectordb {

extern const std::string kLoggerName;
extern std::shared_ptr<spdlog::logger> logger;

void InitLogger(const std::string &logfile);
void DestroyLogger();
void DisableLog();
void EnableLog();

}  // namespace vectordb

#endif  // VECTORDB_UTIL_LOGGER_H