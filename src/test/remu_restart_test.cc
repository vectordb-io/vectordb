#include <gtest/gtest.h>

#include <csignal>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include "clock.h"
#include "coding.h"
#include "common.h"
#include "config.h"
#include "leveldb/db.h"
#include "leveldb/write_batch.h"
#include "logger.h"
#include "raft_server.h"
#include "remu.h"
#include "state_machine.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"
#include "vraft_logger.h"

int32_t g_value_index = 0;
int32_t g_server_index = 0;
int32_t node_num = 3;
int32_t first_repeat_times = 1;

void RemuTick(vraft::Timer *timer) {
  switch (vraft::current_state) {
    // stop first one
    case vraft::kTestState0: {
      vraft::gtest_remu->raft_servers[g_server_index]->raft()->Stop();
      vraft::current_state = vraft::kTestState1;
      timer->set_repeat_times(first_repeat_times);

      std::cout << "stop "
                << (void *)(vraft::gtest_remu->raft_servers[g_server_index]
                                ->raft()
                                .get())
                << std::endl;

      break;
    }

    // start one
    // stop next one
    case vraft::kTestState1: {
      vraft::PrintAndCheck();

      // start one
      for (auto ptr : vraft::gtest_remu->raft_servers) {
        if (!ptr->raft()->started()) {
          int32_t rv = ptr->raft()->Start();
          ASSERT_EQ(rv, 0);

          std::cout << "start " << (void *)(ptr->raft().get()) << std::endl;
        }
      }

      // stop next one
      g_server_index = (g_server_index + 1) % node_num;
      vraft::gtest_remu->raft_servers[g_server_index]->raft()->Stop();

      std::cout << "stop "
                << (void *)(vraft::gtest_remu->raft_servers[g_server_index]
                                ->raft()
                                .get())
                << std::endl;

      timer->RepeatDecr();

      if (timer->repeat_counter() == 0) {
        vraft::current_state = vraft::kTestState2;
      }

      break;
    }

    // start one
    case vraft::kTestState2: {
      vraft::PrintAndCheck();

      for (auto ptr : vraft::gtest_remu->raft_servers) {
        if (!ptr->raft()->started()) {
          int32_t rv = ptr->raft()->Start();
          ASSERT_EQ(rv, 0);

          std::cout << "start " << (void *)(ptr->raft().get()) << std::endl;
        }
      }

      // update repeat counter
      timer->set_repeat_times(5);
      vraft::current_state = vraft::kTestStateEnd;

      break;
    }

    // quit
    case vraft::kTestStateEnd: {
      vraft::PrintAndCheck();

      std::cout << "exit ..." << std::endl;
      vraft::gtest_remu->Stop();
      vraft::gtest_loop->Stop();
    }

    default:
      break;
  }
}

class RemuTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // std::string path = std::string("/tmp/") + __func__;
    vraft::RemuTestSetUp("/tmp/remu_test_dir", RemuTick, nullptr);
  }

  void TearDown() override { vraft::RemuTestTearDown(); }
};

TEST_F(RemuTest, RemuTest) { vraft::RunRemuTest(vraft::gtest_node_num); }

void PrintDesc() {
  if (vraft::gtest_desc) {
    std::string desc;
    char buf[256];

    // parameters
    snprintf(buf, sizeof(buf), "parameters: \n");
    desc.append(buf);

    snprintf(buf, sizeof(buf), "filename:%s \n", __FILE__);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "gtest_enable_pre_vote:%d \n",
             vraft::gtest_enable_pre_vote);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "gtest_interval_check:%d \n",
             vraft::gtest_interval_check);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "gtest_node_num:%d \n", vraft::gtest_node_num);
    desc.append(buf);

    // steps
    int32_t step = 1;
    snprintf(buf, sizeof(buf), "\nsteps: \n");
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: start %d nodes \n", step++,
             vraft::gtest_node_num);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: wait for leader elect \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: check leader stable \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: stop first one \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf),
             "step%d: start one, stop next one "
             "stable \n",
             step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: start one \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: quit \n", step++);
    desc.append(buf);

    std::cout << desc;
    exit(0);
  }
}

int main(int argc, char **argv) {
  vraft::RemuParseConfig(argc, argv);
  PrintDesc();

  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}