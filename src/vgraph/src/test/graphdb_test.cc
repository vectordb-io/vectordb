#include "graphdb.h"
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
namespace vgraph {

class GraphDBTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 使用临时文件作为测试数据库
    db_ = std::make_unique<GraphDB>("/tmp/test_graphdb");
  }

  void TearDown() override {
    db_.reset();
    // 清理测试数据库文件
    system("rm -rf /tmp/test_graphdb");
  }

  std::unique_ptr<GraphDB> db_;
};

// 测试添加节点和边，并打印图结构
TEST_F(GraphDBTest, AddNodesAndEdgesTest) {
  // 添加节点
  EXPECT_TRUE(db_->AddNode(1, R"({"name": "节点1"})"));
  EXPECT_TRUE(db_->AddNode(2, R"({"name": "节点2"})"));
  EXPECT_TRUE(db_->AddNode(3, R"({"name": "节点3"})"));
  EXPECT_TRUE(db_->AddNode(4, R"({"name": "节点4"})"));
  
  // 添加重复节点应该失败
  EXPECT_FALSE(db_->AddNode(1, R"({"name": "重复节点"})"));
  
  // 添加边
  EXPECT_TRUE(db_->AddEdge(1, 2, 1.0, R"({"type": "连接"})"));
  EXPECT_TRUE(db_->AddEdge(2, 3, 2.0, R"({"type": "连接"})"));
  EXPECT_TRUE(db_->AddEdge(3, 4, 3.0, R"({"type": "连接"})"));
  EXPECT_TRUE(db_->AddEdge(1, 4, 10.0, R"({"type": "捷径"})"));
  
  // 添加到不存在节点的边应该失败
  EXPECT_FALSE(db_->AddEdge(1, 5, 1.0, R"({"type": "无效"})"));
  
  // 重定向日志到标准输出，便于观察测试结果
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto logger = std::make_shared<spdlog::logger>("test_logger", console_sink);
  spdlog::set_default_logger(logger);
  
  // 打印图结构
  testing::internal::CaptureStdout();
  db_->Print();
  std::string output = testing::internal::GetCapturedStdout();
  
  // 验证输出中包含所有添加的节点和边
  EXPECT_TRUE(output.find("节点1") != std::string::npos);
  EXPECT_TRUE(output.find("节点2") != std::string::npos);
  EXPECT_TRUE(output.find("节点3") != std::string::npos);
  EXPECT_TRUE(output.find("节点4") != std::string::npos);
  EXPECT_TRUE(output.find("weight: 1") != std::string::npos);
  EXPECT_TRUE(output.find("weight: 2") != std::string::npos);
  EXPECT_TRUE(output.find("weight: 3") != std::string::npos);
  EXPECT_TRUE(output.find("weight: 10") != std::string::npos);

  std::cout << output << std::endl << std::endl;
  std::cout << db_->ToJson().dump(4) << std::endl;
}

// 测试最短路径查找
TEST_F(GraphDBTest, ShortestPathTest) {
  // 构建一个简单的图
  // 1 --1.0-- 2 --2.0-- 3 --3.0-- 4
  // |                            |
  // +----------10.0-------------+
  
  // 添加节点
  EXPECT_TRUE(db_->AddNode(1, R"({"name": "起点"})"));
  EXPECT_TRUE(db_->AddNode(2, R"({"name": "中间点1"})"));
  EXPECT_TRUE(db_->AddNode(3, R"({"name": "中间点2"})"));
  EXPECT_TRUE(db_->AddNode(4, R"({"name": "终点"})"));
  
  // 添加边
  EXPECT_TRUE(db_->AddEdge(1, 2, 1.0, R"({"type": "短路径"})"));
  EXPECT_TRUE(db_->AddEdge(2, 3, 2.0, R"({"type": "短路径"})"));
  EXPECT_TRUE(db_->AddEdge(3, 4, 3.0, R"({"type": "短路径"})"));
  EXPECT_TRUE(db_->AddEdge(1, 4, 10.0, R"({"type": "长路径"})"));
  
  // 测试最短路径查找
  GraphDB::Path path;
  
  // 测试用例1：从1到4的最短路径应该是1->2->3->4
  EXPECT_TRUE(db_->ShortestPath(1, 4, &path));
  EXPECT_EQ(path.nodes.size(), 4);
  EXPECT_EQ(path.nodes[0], 1);
  EXPECT_EQ(path.nodes[1], 2);
  EXPECT_EQ(path.nodes[2], 3);
  EXPECT_EQ(path.nodes[3], 4);
  EXPECT_DOUBLE_EQ(path.total_weight, 6.0);  // 1.0 + 2.0 + 3.0 = 6.0
  
  // 测试用例2：从1到3的最短路径应该是1->2->3
  EXPECT_TRUE(db_->ShortestPath(1, 3, &path));
  EXPECT_EQ(path.nodes.size(), 3);
  EXPECT_EQ(path.nodes[0], 1);
  EXPECT_EQ(path.nodes[1], 2);
  EXPECT_EQ(path.nodes[2], 3);
  EXPECT_DOUBLE_EQ(path.total_weight, 3.0);  // 1.0 + 2.0 = 3.0
  
  // 测试用例3：从1到2的最短路径应该是1->2
  EXPECT_TRUE(db_->ShortestPath(1, 2, &path));
  EXPECT_EQ(path.nodes.size(), 2);
  EXPECT_EQ(path.nodes[0], 1);
  EXPECT_EQ(path.nodes[1], 2);
  EXPECT_DOUBLE_EQ(path.total_weight, 1.0);
  
  // 测试用例4：到不存在的节点应该返回false
  EXPECT_FALSE(db_->ShortestPath(1, 5, &path));
  
  // 测试用例5：从不存在的节点开始应该返回false
  EXPECT_FALSE(db_->ShortestPath(5, 1, &path));
  
  // 打印测试结果
  std::cout << "\n最短路径测试结果：" << std::endl;
  std::cout << "图结构：" << std::endl;
  db_->Print();
  std::cout << "\n路径 1->4：";
  EXPECT_TRUE(db_->ShortestPath(1, 4, &path));
  std::cout << "节点：";
  for (const auto& node : path.nodes) {
    std::cout << node << " ";
  }
  std::cout << "\n总权重：" << path.total_weight << std::endl;
}

// 测试N度邻居查询
TEST_F(GraphDBTest, GetNeighborsWithinNDegreesTest) {
  // 构建一个测试图
  //     2 --- 5
  //    /      |
  //   1       6
  //    \      |
  //     3 --- 4
  
  // 添加节点
  EXPECT_TRUE(db_->AddNode(1, R"({"name": "中心点"})"));
  EXPECT_TRUE(db_->AddNode(2, R"({"name": "一度邻居1"})"));
  EXPECT_TRUE(db_->AddNode(3, R"({"name": "一度邻居2"})"));
  EXPECT_TRUE(db_->AddNode(4, R"({"name": "二度邻居1"})"));
  EXPECT_TRUE(db_->AddNode(5, R"({"name": "二度邻居2"})"));
  EXPECT_TRUE(db_->AddNode(6, R"({"name": "三度邻居"})"));
  
  // 添加边
  EXPECT_TRUE(db_->AddEdge(1, 2, 1.0, R"({"type": "一度连接"})"));
  EXPECT_TRUE(db_->AddEdge(1, 3, 1.0, R"({"type": "一度连接"})"));
  EXPECT_TRUE(db_->AddEdge(2, 5, 1.0, R"({"type": "二度连接"})"));
  EXPECT_TRUE(db_->AddEdge(3, 4, 1.0, R"({"type": "二度连接"})"));
  EXPECT_TRUE(db_->AddEdge(4, 6, 1.0, R"({"type": "三度连接"})"));
  EXPECT_TRUE(db_->AddEdge(5, 6, 1.0, R"({"type": "三度连接"})"));
  
  // 测试用例1：查询0度邻居（只有节点自身）
  auto neighbors = db_->GetNeighborsWithinNDegrees(1, 0);
  EXPECT_EQ(neighbors.size(), 1);
  EXPECT_TRUE(std::find(neighbors.begin(), neighbors.end(), 1) != neighbors.end());
  
  // 测试用例2：查询1度邻居
  neighbors = db_->GetNeighborsWithinNDegrees(1, 1);
  EXPECT_EQ(neighbors.size(), 3);  // 包含自身和两个直接邻居
  EXPECT_TRUE(std::find(neighbors.begin(), neighbors.end(), 1) != neighbors.end());
  EXPECT_TRUE(std::find(neighbors.begin(), neighbors.end(), 2) != neighbors.end());
  EXPECT_TRUE(std::find(neighbors.begin(), neighbors.end(), 3) != neighbors.end());
  
  // 测试用例3：查询2度邻居
  neighbors = db_->GetNeighborsWithinNDegrees(1, 2);
  EXPECT_EQ(neighbors.size(), 5);  // 包含自身、两个一度邻居和两个二度邻居
  EXPECT_TRUE(std::find(neighbors.begin(), neighbors.end(), 4) != neighbors.end());
  EXPECT_TRUE(std::find(neighbors.begin(), neighbors.end(), 5) != neighbors.end());
  
  // 测试用例4：查询3度邻居
  neighbors = db_->GetNeighborsWithinNDegrees(1, 3);
  EXPECT_EQ(neighbors.size(), 6);  // 包含所有节点
  EXPECT_TRUE(std::find(neighbors.begin(), neighbors.end(), 6) != neighbors.end());
  
  // 测试用例5：查询超过实际度数的邻居
  neighbors = db_->GetNeighborsWithinNDegrees(1, 4);
  EXPECT_EQ(neighbors.size(), 6);  // 结果应该和3度邻居相同
  
  // 测试用例6：查询不存在节点的邻居
  neighbors = db_->GetNeighborsWithinNDegrees(7, 1);
  EXPECT_TRUE(neighbors.empty());
  
  // 测试用例7：使用负数度数
  neighbors = db_->GetNeighborsWithinNDegrees(1, -1);
  EXPECT_TRUE(neighbors.empty());
  
  // 打印测试结果
  std::cout << "\nN度邻居测试结果：" << std::endl;
  std::cout << "图结构：" << std::endl;
  db_->Print();
  
  std::cout << "\n从节点1开始的各度数邻居：" << std::endl;
  for (int degree = 0; degree <= 3; ++degree) {
    neighbors = db_->GetNeighborsWithinNDegrees(1, degree);
    std::cout << degree << "度邻居: ";
    for (const auto& node : neighbors) {
      std::cout << node << " ";
    }
    std::cout << std::endl;
  }
}

}  // namespace vgraph
