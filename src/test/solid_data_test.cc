#include "solid_data.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <iostream>

#include "coding.h"

TEST(SolidData, construct) {
  system("rm -rf /tmp/soliddata_test_dir");
  vraft::SolidData solid_data("/tmp/soliddata_test_dir");
  solid_data.Init();
  std::cout << "term: " << solid_data.term() << std::endl;
  std::cout << "vote: " << solid_data.vote() << std::endl;
  EXPECT_EQ(solid_data.term(), static_cast<uint64_t>(1));
  EXPECT_EQ(solid_data.vote(), static_cast<uint64_t>(0));
}

TEST(SolidData, persist) {
  system("rm -rf /tmp/soliddata_test_dir");

  {
    vraft::SolidData solid_data("/tmp/soliddata_test_dir");
    solid_data.Init();
    std::cout << "term: " << solid_data.term() << std::endl;
    std::cout << "vote: " << solid_data.vote() << std::endl;
    EXPECT_EQ(solid_data.term(), static_cast<uint64_t>(1));
    EXPECT_EQ(solid_data.vote(), static_cast<uint64_t>(0));

    solid_data.SetTerm(3);
    EXPECT_EQ(solid_data.term(), static_cast<uint64_t>(3));
    std::cout << "term: " << solid_data.term() << std::endl;

    solid_data.IncrTerm();
    EXPECT_EQ(solid_data.term(), static_cast<uint64_t>(4));
    std::cout << "term: " << solid_data.term() << std::endl;

    solid_data.SetVote(7);
    EXPECT_EQ(solid_data.vote(), static_cast<uint64_t>(7));
    std::cout << "vote: " << solid_data.vote() << std::endl;
  }

  {
    vraft::SolidData solid_data("/tmp/soliddata_test_dir");
    solid_data.Init();
    EXPECT_EQ(solid_data.term(), static_cast<uint64_t>(4));
    EXPECT_EQ(solid_data.vote(), static_cast<uint64_t>(7));

    std::cout << "term: " << solid_data.term() << std::endl;
    std::cout << "vote: " << solid_data.vote() << std::endl;
  }
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}