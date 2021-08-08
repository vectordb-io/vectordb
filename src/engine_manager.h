#ifndef __VECTORDB_ENGINE_MANAGER_H__
#define __VECTORDB_ENGINE_MANAGER_H__

#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
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
    bool AddVEngine(const std::string &replica_name, std::shared_ptr<VEngine> ve);
    bool AddVEngine2(const std::string &path, const VEngineParam &param);
    Status AddVEngine3(std::shared_ptr<Table> t, std::shared_ptr<Partition> p, std::shared_ptr<Replica> r);
    Status LoadEngine(std::shared_ptr<Replica> r);
    Status LoadEngineByPath(const std::string &path);

    std::shared_ptr<VEngine>
    GetVEngine(const std::string &replica_name) const;

    jsonxx::json64 ToJson() const;
    std::string ToString() const;
    std::string ToStringPretty() const;

  private:
    std::map<std::string, std::shared_ptr<VEngine>> vengines_;
    mutable std::mutex mutex_;
};

} // namespace vectordb

#endif
