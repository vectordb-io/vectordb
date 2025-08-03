#include "vraft_logger.h"

namespace vraft {

vraft::LoggerOptions vraft_logger_options{"vraft", false, 1, 8192,
                                          vraft::kLoggerTrace};
// vraft::Logger vraft_logger("./log/vraft.log", vraft_logger_options);

vraft::Logger vraft_logger;

}  // namespace vraft
