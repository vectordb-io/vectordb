#ifndef __VECTORDB_ENGINE_MANAGER_H__
#define __VECTORDB_ENGINE_MANAGER_H__

#include <map>
#include <vector>
#include <memory>
#include <string>
#include "status.h"
#include "vengine.h"

namespace vectordb {

class EngineManager {
  public:
    EngineManager() = default;
    EngineManager(const EngineManager&) = delete;
    EngineManager& operator=(const EngineManager&) = delete;
    ~EngineManager() = default;

    Status Init();
    void AddVEngine(const std::string &replica_name, std::shared_ptr<VEngine> ve);

    std::shared_ptr<VEngine>
    GetVEngine(const std::string &replica_name) const;

  private:
    std::map<std::string, std::shared_ptr<VEngine>> vengines_;
};

} // namespace vectordb

#endif
