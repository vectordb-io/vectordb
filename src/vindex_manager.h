#ifndef __VECTORDB_VINDEX_MANAGER_H__
#define __VECTORDB_VINDEX_MANAGER_H__

#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <functional>
#include "vindex.h"
#include "vindex_annoy.h"
#include "vindex_transparent.h"
#include "status.h"

namespace vectordb {

class VIndexFactory {
  public:
    static std::shared_ptr<VIndex>
    Create(const std::string &index_type, const std::string &path, VEngine* vengine, void *param) {
        std::shared_ptr<VIndex> index_sp;
        if (index_type == VINDEX_TYPE_ANNOY) {
            AnnoyParam *annoy_param = static_cast<AnnoyParam*>(param);
            index_sp = std::make_shared<VIndexAnnoy>(path, vengine, annoy_param);
            assert(index_sp);

        } else if (index_type == VINDEX_TYPE_TRANSPARENT) {
            TransparentParam *transparent_param = static_cast<TransparentParam*>(param);
            index_sp = std::make_shared<VIndexTransparent>(path, vengine, transparent_param);
            assert(index_sp);

        } else if (index_type == VINDEX_TYPE_KNN_GRAPH) {

        } else if (index_type == VINDEX_TYPE_FAISS) {

        }

        return index_sp;
    }

    static std::shared_ptr<VIndex>
    Create(const std::string &index_type, const std::string &path, VEngine* vengine) {
        std::shared_ptr<VIndex> index_sp;
        if (index_type == VINDEX_TYPE_ANNOY) {
            index_sp = std::make_shared<VIndexAnnoy>(path, vengine);
            assert(index_sp);

        } else if (index_type == VINDEX_TYPE_TRANSPARENT) {
            index_sp = std::make_shared<VIndexTransparent>(path, vengine);
            assert(index_sp);

        } else if (index_type == VINDEX_TYPE_KNN_GRAPH) {

        } else if (index_type == VINDEX_TYPE_FAISS) {

        }

        return index_sp;
    }
};

class VIndexManager {
  public:
    VIndexManager(const std::string path, VEngine *vengine);
    VIndexManager(const VIndexManager&) = delete;
    VIndexManager& operator=(const VIndexManager&) = delete;
    ~VIndexManager();

    Status Init();
    bool HasIndex() const;
    Status Add(std::shared_ptr<VIndex> index);
    Status Del(const std::string &name);
    std::shared_ptr<VIndex> GetByName(const std::string &name);
    std::shared_ptr<VIndex> GetNewestByType(const std::string &index_type);
    std::shared_ptr<VIndex> GetNewest();
    Status ForEachIndex(std::function<Status(std::shared_ptr<VIndex>)> func);

    jsonxx::json64 ToJson() const;
    std::string ToString() const;
    std::string ToStringPretty() const;

    std::string path() const {
        return path_;
    }

  private:
    std::string path_;
    VEngine *vengine_;

    std::map<std::string, std::shared_ptr<VIndex>> indices_by_name_;
    std::map<time_t, std::shared_ptr<VIndex>> indices_by_time_;
    mutable std::mutex mutex_;
};

} // namespace vectordb

#endif
