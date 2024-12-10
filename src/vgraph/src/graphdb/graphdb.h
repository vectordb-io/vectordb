#ifndef VGRAPH_GRAPHDB_GRAPHDB_H_
#define VGRAPH_GRAPHDB_GRAPHDB_H_

#include <leveldb/db.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <queue>
#include "slice.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
namespace vgraph {

/**
 * @brief 图数据库存储引擎类
 * 
 * 功能特性:
 * - 支持添加节点和边
 * - 支持查询两点间最短路径
 * - 支持查询节点的N度邻居
 * - 使用leveldb作为底层存储引擎
 * - 支持错误处理和异常情况
 */
class GraphDB {
 public:
  // 节点ID类型
  using NodeId = int64_t;
  
  // 边的权重类型
  using Weight = double;
  
  // 路径类型
  struct Path {
    std::vector<NodeId> nodes;  // 路径上的节点
    Weight total_weight;        // 路径总权重

    nlohmann::json ToJson() const {
      return nlohmann::json{{"nodes", nodes}, {"total_weight", total_weight}};
    }
  };

  /**
   * @brief 图数据库中的节点结构
   * 
   * 包含节点的基本信息：
   * - 节点ID：唯一标识符
   * - 属性：JSON格式存储的节点属性
   * - 邻居信息：存储与该节点直接相连的所有节点及边的权重
   */
  struct Node {
    NodeId id;                                      // 节点的唯一标识符
    std::string properties;                         // 节点属性(JSON格式)
    std::unordered_map<NodeId, Weight> neighbors;   // 邻接表：<邻居节点ID, 边权重>
  };

  /**
   * @brief 构造函数
   * @param db_path 数据库文件路径
   * @throws std::runtime_error 如果数据库打开失败
   */
  explicit GraphDB(const std::string& db_path);

  /**
   * @brief 析构函数
   */
  ~GraphDB();

  /**
   * @brief 添加节点
   * @param node_id 节点ID
   * @param properties 节点属性(JSON格式)
   * @return 是否添加成功
   */
  bool AddNode(NodeId node_id, const std::string& properties);

  /**
   * @brief 添加边
   * @param from_id 起始节点ID
   * @param to_id 目标节点ID
   * @param weight 边的权重
   * @param properties 边的属性(JSON格式)
   * @return 是否添加成功
   */
  bool AddEdge(NodeId from_id, NodeId to_id, Weight weight,
               const std::string& properties);

  /**
   * @brief 查询两点间的最短路径
   * @param from_id 起始节点ID
   * @param to_id 目标节点ID
   * @param path 输出参数，存储找到的最短路径
   * @return 是否找到路径
   */
  bool ShortestPath(NodeId from_id, NodeId to_id, Path* path);

  /**
   * @brief 查询节点的N度邻居
   * @param node_id 起始节点ID
   * @param n 度数
   * @return 邻居节点ID列表
   */
  std::vector<NodeId> GetNeighborsWithinNDegrees(NodeId node_id, int n);

  /**
   * @brief 打印图数据库中的所有节点与边
   */
  void Print() const;

  /**
   * @brief 将图数据库转换为JSON格式
   * @return JSON格式的图数据
   */
  nlohmann::json ToJson() const;

 private:
  // 禁止拷贝和赋值
  GraphDB(const GraphDB&) = delete;
  GraphDB& operator=(const GraphDB&) = delete;

  /**
   * @brief 检查节点是否存在
   * @param node_id 节点ID
   * @return 节点是否存在
   */
  bool NodeExists(NodeId node_id) const;

  /**
   * @brief 获取节点的直接邻居
   * @param node_id 节点ID
   * @return 邻居节点ID和权重的映射
   */
  std::unordered_map<NodeId, Weight> GetDirectNeighbors(NodeId node_id) const;

  // LevelDB实例，用于存储和检索图数据
  std::unique_ptr<leveldb::DB> db_;
};

}  // namespace vgraph

#endif  // VGRAPH_GRAPHDB_GRAPHDB_H_
