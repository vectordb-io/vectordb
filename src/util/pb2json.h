#ifndef UTIL_PB2JSON_H
#define UTIL_PB2JSON_H

#include "common.h"
#include "nlohmann/json.hpp"
#include "vdb.pb.h"

namespace vectordb {

json FlatParamToJson(const vdb::FlatParam &param);

json HnswParamToJson(const vdb::HnswParam &param);

json IndexInfoToJson(const vdb::IndexInfo &param);

json IndexParamToJson(const vdb::IndexParam &param);

json TableInfoToJson(const vdb::TableInfo &param);

json TableParamToJson(const vdb::TableParam &param);

}  // namespace vectordb

#endif