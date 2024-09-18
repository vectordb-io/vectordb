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

void RemuTick(vraft::Timer *timer) {
  switch (vraft::current_state) {
    // stop first one
    case vraft::kTestState0: {
      vraft::gtest_remu
          ->raft_servers[(g_server_index++) % vraft::gtest_node_num]
          ->raft()
          ->Stop();
      vraft::current_state = vraft::kTestState1;
      timer->set_repeat_times(20);
      break;
    }

    // propose 10 values
    // start one
    // stop next one
    case vraft::kTestState1: {
      vraft::PrintAndCheck();

      // propose 10 values
      for (auto ptr : vraft::gtest_remu->raft_servers) {
        for (int i = 0; i < 10; ++i) {
          char value_buf[128];
          snprintf(value_buf, sizeof(value_buf), "key_%d:value_%d",
                   g_value_index, g_value_index);
          g_value_index++;
          ptr->raft()->Propose(std::string(value_buf), nullptr);
        }
      }

      // start one
      for (auto ptr : vraft::gtest_remu->raft_servers) {
        if (!ptr->raft()->started()) {
          int32_t rv = ptr->raft()->Start();
          ASSERT_EQ(rv, 0);
        }
      }

      // stop next one
      vraft::gtest_remu
          ->raft_servers[(g_server_index++) % vraft::gtest_node_num]
          ->raft()
          ->Stop();

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
        }
      }

      // update repeat counter
      timer->set_repeat_times(5);
      vraft::current_state = vraft::kTestState3;

      break;
    }

    // wait 5s, for log catch up
    case vraft::kTestState3: {
      vraft::PrintAndCheck();

      timer->RepeatDecr();
      if (timer->repeat_counter() == 0) {
        vraft::current_state = vraft::kTestState4;
      }

      break;
    }

    // check sm value
    case vraft::kTestState4: {
      for (auto ptr : vraft::gtest_remu->raft_servers) {
        std::cout << "------------------------------------" << std::endl;
        std::cout << "LastIndex: " << ptr->raft()->sm()->LastIndex()
                  << std::endl;
        std::cout << "LastTerm: " << ptr->raft()->sm()->LastTerm() << std::endl;
        for (int i = 1; i <= g_value_index; ++i) {
          char key[32];
          snprintf(key, sizeof(key), "key_%d", i);
          std::string value;
          vraft::TestSM *psm = (vraft::TestSM *)(ptr->raft()->sm().get());
          int32_t rv = psm->Get(std::string(key), value);
          if (rv == 1) {
            std::cout << key << " --- " << value << std::endl;
          } else {
            assert(rv == 0);
            std::cout << key << " --- "
                      << "not found" << std::endl;
          }
        }
      }

      vraft::current_state = vraft::kTestState5;
      break;
    }

    // check log consistant
    case vraft::kTestState5: {
      uint32_t checksum =
          vraft::gtest_remu->raft_servers[0]->raft()->log().LastCheck();
      printf("====log checksum:%X \n\n", checksum);
      for (auto &rs : vraft::gtest_remu->raft_servers) {
        auto sptr = rs->raft();
        uint32_t checksum2 = sptr->log().LastCheck();
        ASSERT_EQ(checksum, checksum2);
      }

      vraft::current_state = vraft::kTestState6;

      break;
    }

    // check sm consistant
    case vraft::kTestState6: {
      vraft::TestSM *psm =
          (vraft::TestSM
               *)(vraft::gtest_remu->raft_servers[0]->raft()->sm().get());
      int32_t apply_count = psm->apply_count();
      int32_t check_sum = psm->check_sum();
      std::string all_values = psm->all_values();
      std::cout << "first one:" << std::endl;
      std::cout << psm->ToJsonString(false, false) << std::endl;

      for (auto &rs : vraft::gtest_remu->raft_servers) {
        auto sptr = rs->raft();
        vraft::TestSM *psm2 = (vraft::TestSM *)(rs->raft()->sm().get());
        std::cout << "compare:" << (void *)(psm2) << std::endl;
        std::cout << psm2->ToJsonString(false, false) << std::endl;

        int32_t apply_count2 = psm2->apply_count();
        int32_t check_sum2 = psm2->check_sum();
        std::string all_values2 = psm2->all_values();
        ASSERT_EQ(apply_count, apply_count2);
        ASSERT_EQ(check_sum, check_sum2);
        ASSERT_EQ(all_values, all_values2);
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

    snprintf(buf, sizeof(buf), "step%d: propose 10 values \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: stop and start one by one \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: check sm value \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: check log consistant \n", step++);
    desc.append(buf);

    snprintf(buf, sizeof(buf), "step%d: check sm consistant \n", step++);
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