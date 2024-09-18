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
#include "logger.h"
#include "raft_server.h"
#include "remu.h"
#include "state_machine.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"
#include "vraft_logger.h"

void RemuTick(vraft::Timer *timer) {
  switch (vraft::current_state) {
    // wait until elect leader
    case vraft::kTestState0: {
      vraft::PrintAndCheck();

      int32_t leader_num = 0;
      for (auto ptr : vraft::gtest_remu->raft_servers) {
        if (ptr->raft()->state() == vraft::STATE_LEADER &&
            ptr->raft()->started()) {
          leader_num++;
        }
      }

      if (leader_num == 1) {
        timer->set_repeat_times(5);
        vraft::current_state = vraft::kTestState1;
      }

      break;
    }

    // propose 5 values
    case vraft::kTestState1: {
      vraft::PrintAndCheck();

      for (auto &rs : vraft::gtest_remu->raft_servers) {
        auto sptr = rs->raft();
        if (sptr && sptr->state() == vraft::STATE_LEADER && sptr->started()) {
          char value_buf[128];
          snprintf(value_buf, sizeof(value_buf), "key_%ld:value_%ld",
                   timer->repeat_counter(), timer->repeat_counter());
          int32_t rv = sptr->Propose(std::string(value_buf), nullptr);
          if (rv == 0) {
            printf("%s propose value: %s\n\n", sptr->Me().ToString().c_str(),
                   value_buf);
          }
          timer->RepeatDecr();
        }
      }

      if (timer->repeat_counter() == 0) {
        timer->set_repeat_times(5);
        vraft::current_state = vraft::kTestState2;
      }
      break;
    }

    // wait 5s, for log catch up
    case vraft::kTestState2: {
      vraft::PrintAndCheck();

      timer->RepeatDecr();
      if (timer->repeat_counter() == 0) {
        vraft::current_state = vraft::kTestState3;
      }

      break;
    }

    // check log consistant
    case vraft::kTestState3: {
      uint32_t checksum =
          vraft::gtest_remu->raft_servers[0]->raft()->log().LastCheck();
      printf("====log checksum:%X \n\n", checksum);
      for (auto &rs : vraft::gtest_remu->raft_servers) {
        auto sptr = rs->raft();
        uint32_t checksum2 = sptr->log().LastCheck();
        ASSERT_EQ(checksum, checksum2);
      }

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
    vraft::RemuTestSetUp("/tmp/remu_test_dir", RemuTick, vraft::CreateSM);
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

    snprintf(buf, sizeof(buf), "step%d: propose 5 values \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: wait 5s, for log catch up \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: check log consistant \n", step++);
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