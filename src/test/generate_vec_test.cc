#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "raft.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"
#include "vengine.h"

int main(int argc, char **argv) {
  vraft::CodingInit();
  int32_t dim = 10;
  int32_t lines = 100;

  if (argc == 3) {
    dim = atoi(argv[1]);
    lines = atoi(argv[2]);
  }

  for (int32_t i = 0; i < lines; ++i) {
    char buf[512];
    snprintf(buf, sizeof(buf), "key_%d", i);
    std::string key = buf;

    std::string vec_str;
    if (dim > 0) {
      for (int32_t j = 0; j < dim - 1; ++j) {
        float f32 = vraft::RandomFloat(1);
        snprintf(buf, sizeof(buf), "%f, ", f32);
        vec_str.append(buf);
      }
      float f32 = vraft::RandomFloat(1);
      snprintf(buf, sizeof(buf), "%f", f32);
      vec_str.append(buf);
    }

    snprintf(buf, sizeof(buf), "attach_value_%d", i);
    std::string attach_value = buf;

    std::cout << key << "; " << vec_str << "; " << attach_value << std::endl;
  }

  return 0;
}