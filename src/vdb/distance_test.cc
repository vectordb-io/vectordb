#include "distance.h"

#include <gtest/gtest.h>

#include <vector>

namespace vectordb {

TEST(DistanceTest, L2Test) {
  // 测试相同向量
  std::vector<float> v1 = {1.0, 2.0, 3.0};
  std::vector<float> v2 = {1.0, 2.0, 3.0};
  EXPECT_FLOAT_EQ(L2(v1, v2), 0.0);

  // 测试不同向量
  std::vector<float> v3 = {4.0, 5.0, 6.0};
  EXPECT_FLOAT_EQ(L2(v1, v3),
                  27.0);  // (4-1)^2 + (5-2)^2 + (6-3)^2 = 9 + 9 + 9 = 27

  // 测试负值
  std::vector<float> v4 = {-1.0, -2.0, -3.0};
  EXPECT_FLOAT_EQ(
      L2(v1, v4),
      56.0);  // (1-(-1))^2 + (2-(-2))^2 + (3-(-3))^2 = 4 + 16 + 36 = 56

  // 测试零向量
  std::vector<float> v5 = {0.0, 0.0, 0.0};
  EXPECT_FLOAT_EQ(L2(v5, v5), 0.0);
  EXPECT_FLOAT_EQ(L2(v1, v5), 14.0);  // 1^2 + 2^2 + 3^2 = 14
}

TEST(DistanceTest, InnerProductTest) {
  // 测试相同向量
  std::vector<float> v1 = {1.0, 2.0, 3.0};
  std::vector<float> v2 = {1.0, 2.0, 3.0};
  EXPECT_FLOAT_EQ(InnerProduct(v1, v2), 14.0);  // 1*1 + 2*2 + 3*3 = 14

  // 测试不同向量
  std::vector<float> v3 = {4.0, 5.0, 6.0};
  EXPECT_FLOAT_EQ(InnerProduct(v1, v3),
                  32.0);  // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32

  // 测试正交向量
  std::vector<float> v4 = {3.0, -1.0, 0.0};
  std::vector<float> v5 = {1.0, 3.0, 0.0};
  EXPECT_FLOAT_EQ(InnerProduct(v4, v5), 0.0);  // 3*1 + (-1)*3 + 0*0 = 3 - 3 = 0

  // 测试零向量
  std::vector<float> v6 = {0.0, 0.0, 0.0};
  EXPECT_FLOAT_EQ(InnerProduct(v6, v6), 0.0);
  EXPECT_FLOAT_EQ(InnerProduct(v1, v6), 0.0);
}

TEST(DistanceTest, HnswLibL2Test) {
  std::vector<float> v1 = {1.0, 1.0, 1.0};
  std::vector<float> v2 = {2.0, 2.0, 3.0};
  float d1 = vectordb::L2(v1, v2);
  float d2 = vectordb::HnswLibL2(v1, v2);
  std::cout << "d1: " << d1 << ", d2: " << d2 << std::endl;
  EXPECT_FLOAT_EQ(d1, d2);
}

TEST(DistanceTest, HnswLibInnerProductTest) {
  std::vector<float> v1 = {1.0, 1.0, 1.0};
  std::vector<float> v2 = {2.0, 2.0, 3.0};
  float d1 = vectordb::InnerProduct(v1, v2);
  float d2 = vectordb::IPDistance(v1, v2);
  float d3 = vectordb::HnswLibIPDistance(v1, v2);
  std::cout << "d1: " << d1 << ", d2: " << d2 << ", d3: " << d3 << std::endl;
  EXPECT_FLOAT_EQ(d2, d3);
}

TEST(DistanceTest, HnswLibIPDistanceTest) {
  std::vector<float> vector1(128, 0.1f);
  std::vector<float> vector2(128, 0.2f);
  std::vector<float> vector3(128, 0.3f);

  {
    float d1 = vectordb::HnswLibIPDistance(vector1, vector1);
    float d2 = vectordb::HnswLibIPDistance(vector2, vector2);
    float d3 = vectordb::HnswLibIPDistance(vector3, vector3);

    float d4 = vectordb::HnswLibIPDistance(vector1, vector2);
    float d5 = vectordb::HnswLibIPDistance(vector1, vector3);
    float d6 = vectordb::HnswLibIPDistance(vector2, vector3);
    std::cout << "v1 - v1: " << d1 << std::endl;
    std::cout << "v2 - v2: " << d2 << std::endl;
    std::cout << "v3 - v3: " << d3 << std::endl;
    std::cout << "v1 - v2: " << d4 << std::endl;
    std::cout << "v1 - v3: " << d5 << std::endl;
    std::cout << "v2 - v3: " << d6 << std::endl;
  }
  //-------------------------------------------------------------------------------------

  vectordb::Normalize(vector1);
  vectordb::Normalize(vector2);
  vectordb::Normalize(vector3);

  {
    float d1 = vectordb::HnswLibIPDistance(vector1, vector1);
    float d2 = vectordb::HnswLibIPDistance(vector2, vector2);
    float d3 = vectordb::HnswLibIPDistance(vector3, vector3);

    float d4 = vectordb::HnswLibIPDistance(vector1, vector2);
    float d5 = vectordb::HnswLibIPDistance(vector1, vector3);
    float d6 = vectordb::HnswLibIPDistance(vector2, vector3);
    std::cout << "v1 - v1: " << d1 << std::endl;
    std::cout << "v2 - v2: " << d2 << std::endl;
    std::cout << "v3 - v3: " << d3 << std::endl;
    std::cout << "v1 - v2: " << d4 << std::endl;
    std::cout << "v1 - v3: " << d5 << std::endl;
    std::cout << "v2 - v3: " << d6 << std::endl;
  }
}

TEST(DistanceTest, NormalizeTest) {
  std::vector<float> v1 = {1.0, 2.0, 3.0};
  float n1 = vectordb::Norm(v1);
  std::cout << "n1: " << n1 << std::endl;
  vectordb::Normalize(v1);
  float n2 = vectordb::Norm(v1);
  std::cout << "v1: " << v1[0] << ", " << v1[1] << ", " << v1[2] << std::endl;
  std::cout << "n2: " << n2 << std::endl;
}

}  // namespace vectordb

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}