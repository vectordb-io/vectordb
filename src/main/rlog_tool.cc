#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

#include "coding.h"
#include "raft_log.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cout << argv[0] << " rlog_path" << std::endl;
    return 0;
  }
  vraft::CodingInit();

  std::string path = argv[1];
  vraft::RaftLog rlog(path);
  rlog.Init();
  std::cout << rlog.ToJsonString(false, true) << std::endl;

  return 0;
}