#ifndef __VECTORDB_ENGINE_MANAGER_H__
#define __VECTORDB_ENGINE_MANAGER_H__

#include <map>
#include <vector>
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
    VEngine* GetVEngine(const std::string &replica_name) const;
    void AddVEngine(const std::string &replica_name, VEngine*);

  private:
    std::map<std::string, VEngine*> vengines_;
};

} // namespace vectordb

#endif
