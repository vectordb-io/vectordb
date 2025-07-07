#ifndef VDB_DISTANCE_H
#define VDB_DISTANCE_H

#include <vector>

#include "hnswlib/hnswlib.h"

namespace vectordb {

float L2(const std::vector<float> &v1, const std::vector<float> &v2);

float InnerProduct(const std::vector<float> &v1, const std::vector<float> &v2);

float IPDistance(const std::vector<float> &v1, const std::vector<float> &v2);

float HnswLibL2(const std::vector<float> &v1, const std::vector<float> &v2);

float HnswLibIPDistance(const std::vector<float> &v1,
                        const std::vector<float> &v2);

void Normalize(std::vector<float> &v);

float Norm(const std::vector<float> &v);

}  // namespace vectordb

#endif