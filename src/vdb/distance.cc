#include "distance.h"
#

namespace vectordb {

float L2(const std::vector<float> &v1, const std::vector<float> &v2) {
  assert(v1.size() == v2.size());

  float distance = 0.0;
  for (size_t i = 0; i < v1.size(); i++) {
    distance += (v1[i] - v2[i]) * (v1[i] - v2[i]);
  }
  return distance;
}

float InnerProduct(const std::vector<float> &v1, const std::vector<float> &v2) {
  assert(v1.size() == v2.size());

  float distance = 0.0;
  for (size_t i = 0; i < v1.size(); i++) {
    distance += v1[i] * v2[i];
  }
  return distance;
}

float IPDistance(const std::vector<float> &v1, const std::vector<float> &v2) {
  return 1 - InnerProduct(v1, v2);
}

float HnswLibL2(const std::vector<float> &v1, const std::vector<float> &v2) {
  assert(v1.size() == v2.size());

  int32_t dim = v1.size();
  hnswlib::L2Space space(dim);
  auto dist_func = space.get_dist_func();
  auto dist_param = space.get_dist_func_param();
  float distance = dist_func(v1.data(), v2.data(), dist_param);
  return distance;
}

float HnswLibIPDistance(const std::vector<float> &v1,
                        const std::vector<float> &v2) {
  assert(v1.size() == v2.size());
  int32_t dim = v1.size();
  hnswlib::InnerProductSpace space(dim);
  auto dist_func = space.get_dist_func();
  auto dist_param = space.get_dist_func_param();
  float distance = dist_func(v1.data(), v2.data(), dist_param);
  return distance;
}

void Normalize(std::vector<float> &v) {
  float norm = 0.0;
  for (size_t i = 0; i < v.size(); i++) {
    norm += v[i] * v[i];
  }
  norm = std::sqrt(norm);

  if (norm > 0) {
    for (size_t i = 0; i < v.size(); i++) {
      v[i] /= norm;
    }
  }
}

float Norm(const std::vector<float> &v) {
  float norm = 0.0;
  for (size_t i = 0; i < v.size(); i++) {
    norm += v[i] * v[i];
  }
  return std::sqrt(norm);
}

}  // namespace vectordb