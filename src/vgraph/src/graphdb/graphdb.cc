#include "graphdb.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <queue>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <iostream>

namespace vgraph {

namespace {
// 用于构造leveldb中的key的前缀
constexpr char kNodeKeyPrefix[] = "n:";  // 节点key前缀
constexpr char kEdgeKeyPrefix[] = "e:";  // 边key前缀

// 构造节点在leveldb中的key
std::string MakeNodeKey(GraphDB::NodeId node_id) {
  return kNodeKeyPrefix + std::to_string(node_id);
}

// 构造边在leveldb中的key
std::string MakeEdgeKey(GraphDB::NodeId from_id, GraphDB::NodeId to_id) {
  return kEdgeKeyPrefix + std::to_string(from_id) + ":" + std::to_string(to_id);
}

// 将Node序列化为JSON字符串
std::string SerializeNode(const GraphDB::Node& node) {
  nlohmann::json j;
  j["id"] = node.id;
  j["properties"] = node.properties;
  j["neighbors"] = node.neighbors;
  return j.dump();
}

// 从JSON字符串反序列化Node
GraphDB::Node DeserializeNode(const std::string& json_str) {
  GraphDB::Node node;
  auto j = nlohmann::json::parse(json_str);
  node.id = j["id"].get<GraphDB::NodeId>();
  node.properties = j["properties"].get<std::string>();
  node.neighbors = j["neighbors"].get<std::unordered_map<GraphDB::NodeId, GraphDB::Weight>>();
  return node;
}

} // anonymous namespace

GraphDB::GraphDB(const std::string& db_path) {
  leveldb::Options options;
  options.create_if_missing = true;
  
  leveldb::DB* db = nullptr;
  leveldb::Status status = leveldb::DB::Open(options, db_path, &db);
  
  if (!status.ok()) {
    throw std::runtime_error("Failed to open database: " + status.ToString());
  }
  
  db_.reset(db);
}

GraphDB::~GraphDB() = default;

bool GraphDB::AddNode(NodeId node_id, const std::string& properties) {
  spdlog::info("Adding node: id={}, properties={}", node_id, properties);
  
  if (NodeExists(node_id)) {
    spdlog::warn("Node already exists: id={}", node_id);
    return false;
  }

  try {
    // 创建新节点
    Node node;
    node.id = node_id;
    node.properties = properties;
    
    // 序列化并存储节点
    std::string node_key = MakeNodeKey(node_id);
    std::string node_value = SerializeNode(node);
    
    leveldb::Status status = db_->Put(leveldb::WriteOptions(), node_key, node_value);
    
    if (!status.ok()) {
      spdlog::error("Failed to add node {}: {}", node_id, status.ToString());
      return false;
    }
    
    spdlog::info("Successfully added node: id={}", node_id);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to add node: id={}, error={}", node_id, e.what());
    return false;
  }
}

bool GraphDB::AddEdge(NodeId from_id, NodeId to_id, Weight weight,
                     const std::string& properties) {
  spdlog::info("Adding edge: from={}, to={}, weight={}, properties={}", 
               from_id, to_id, weight, properties);

  if (!NodeExists(from_id) || !NodeExists(to_id)) {
    spdlog::warn("Source or target node does not exist: from={}, to={}", 
                 from_id, to_id);
    return false;
  }

  try {
    // 获取起始节点
    std::string from_key = MakeNodeKey(from_id);
    std::string value;
    leveldb::Status status = db_->Get(leveldb::ReadOptions(), from_key, &value);
    
    if (!status.ok()) {
      spdlog::error("Failed to get node {}: {}", from_id, status.ToString());
      return false;
    }
    
    Node from_node = DeserializeNode(value);
    
    // 更新邻接表
    from_node.neighbors[to_id] = weight;
    
    // 保存更新后的节点
    std::string new_value = SerializeNode(from_node);
    status = db_->Put(leveldb::WriteOptions(), from_key, new_value);
    
    if (!status.ok()) {
      spdlog::error("Failed to update node {}: {}", from_id, status.ToString());
      return false;
    }
    
    // 存储边的属性
    std::string edge_key = MakeEdgeKey(from_id, to_id);
    status = db_->Put(leveldb::WriteOptions(), edge_key, properties);
    
    if (!status.ok()) {
      spdlog::error("Failed to store edge properties: {}", status.ToString());
      return false;
    }
    
    spdlog::info("Successfully added edge: from={}, to={}", from_id, to_id);
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to add edge: from={}, to={}, error={}", 
                  from_id, to_id, e.what());
    return false;
  }
}

bool GraphDB::ShortestPath(NodeId from_id, NodeId to_id, Path* path) {
  spdlog::info("Finding shortest path: from={}, to={}", from_id, to_id);

  if (!NodeExists(from_id) || !NodeExists(to_id)) {
    spdlog::warn("Source or target node does not exist: from={}, to={}", 
                 from_id, to_id);
    return false;
  }

  try {
    // 使用Dijkstra算法查找最短路径
    std::unordered_map<NodeId, Weight> distances;
    std::unordered_map<NodeId, NodeId> previous;
    std::priority_queue<std::pair<Weight, NodeId>,
                       std::vector<std::pair<Weight, NodeId>>,
                       std::greater<>> pq;
    
    // 初始化距离
    distances[from_id] = 0;
    pq.push({0, from_id});
    
    while (!pq.empty()) {
      auto [dist, current] = pq.top();
      pq.pop();
      
      if (current == to_id) {
        break;
      }
      
      if (dist > distances[current]) {
        continue;
      }
      
      // 遍历邻居
      auto neighbors = GetDirectNeighbors(current);
      for (const auto& [neighbor, weight] : neighbors) {
        Weight new_dist = dist + weight;
        
        if (distances.find(neighbor) == distances.end() ||
            new_dist < distances[neighbor]) {
          distances[neighbor] = new_dist;
          previous[neighbor] = current;
          pq.push({new_dist, neighbor});
        }
      }
    }
    
    // 如果没有找到路径
    if (previous.find(to_id) == previous.end()) {
      spdlog::warn("No path found: from={}, to={}", from_id, to_id);
      return false;
    }
    
    // 构造路径
    path->nodes.clear();
    path->total_weight = distances[to_id];
    
    for (NodeId current = to_id; current != from_id;
         current = previous[current]) {
      path->nodes.push_back(current);
    }
    path->nodes.push_back(from_id);
    
    // 反转路径使其从起点开始
    std::reverse(path->nodes.begin(), path->nodes.end());
    
    spdlog::info("Found path: from={}, to={}, length={}, total_weight={}, path={}", 
                 from_id, to_id, path->nodes.size(), path->total_weight, path->ToJson().dump());
    return true;
  } catch (const std::exception& e) {
    spdlog::error("Failed to find path: from={}, to={}, error={}", 
                  from_id, to_id, e.what());
    return false;
  }
}

std::vector<GraphDB::NodeId> GraphDB::GetNeighborsWithinNDegrees(
    NodeId node_id, int n) {
  spdlog::info("Getting neighbors within {} degrees: node={}", n, node_id);

  // 检查节点是否存在
  if (!NodeExists(node_id)) {
    spdlog::warn("Node does not exist: id={}", node_id);
    return {};
  }

  // 处理无效的度数
  if (n < 0) {
    spdlog::warn("Invalid degree value: {}", n);
    return {};
  }

  try {
    std::unordered_set<NodeId> result;
    std::queue<std::pair<NodeId, int>> q;
    
    // 从起始节点开始BFS
    q.push({node_id, 0});
    result.insert(node_id);
    
    while (!q.empty()) {
      auto [current, degree] = q.front();
      q.pop();
      
      if (degree >= n) {
        continue;
      }
      
      // 获取当前节点的直接邻居
      auto neighbors = GetDirectNeighbors(current);
      for (const auto& [neighbor, _] : neighbors) {
        if (result.insert(neighbor).second) {
          q.push({neighbor, degree + 1});
        }
      }
    }
    
    spdlog::info("Found {} neighbors within {} degrees for node {}", 
                 result.size(), n, node_id);
    return std::vector<NodeId>(result.begin(), result.end());
  } catch (const std::exception& e) {
    spdlog::error("Failed to get neighbors: node={}, degrees={}, error={}", 
                  node_id, n, e.what());
    return {};
  }
}

bool GraphDB::NodeExists(NodeId node_id) const {
  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(),
                                  MakeNodeKey(node_id), &value);
  return status.ok();
}

std::unordered_map<GraphDB::NodeId, GraphDB::Weight>
GraphDB::GetDirectNeighbors(NodeId node_id) const {
  std::string value;
  leveldb::Status status = db_->Get(leveldb::ReadOptions(),
                                  MakeNodeKey(node_id), &value);
  
  if (!status.ok()) {
    return {};
  }
  
  Node node = DeserializeNode(value);
  return node.neighbors;
}

void GraphDB::Print() const {
  std::unique_ptr<leveldb::Iterator> it(
      db_->NewIterator(leveldb::ReadOptions()));
      
  std::cout << "Graph contents:\n";
  
  // 打印所有节点
  for (it->Seek(kNodeKeyPrefix); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (key.compare(0, strlen(kNodeKeyPrefix), kNodeKeyPrefix) != 0) {
      break;
    }
    
    Node node = DeserializeNode(it->value().ToString());
    std::cout << "Node " << node.id << ": " << node.properties << "\n";
    for (const auto& [neighbor, weight] : node.neighbors) {
      std::cout << "  -> Node " << neighbor << " (weight: " << weight << ")\n";
    }
  }
  
  // 打印所有边的属性
  for (it->Seek(kEdgeKeyPrefix); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (key.compare(0, strlen(kEdgeKeyPrefix), kEdgeKeyPrefix) != 0) {
      break;
    }
    
    std::cout << "Edge " << key << ": " << it->value().ToString() << "\n";
  }
}

nlohmann::json GraphDB::ToJson() const {
  nlohmann::json result;
  std::unique_ptr<leveldb::Iterator> it(
      db_->NewIterator(leveldb::ReadOptions()));
      
  // 收集所有节点信息
  nlohmann::json nodes = nlohmann::json::array();
  for (it->Seek(kNodeKeyPrefix); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (key.compare(0, strlen(kNodeKeyPrefix), kNodeKeyPrefix) != 0) {
      break;
    }
    
    Node node = DeserializeNode(it->value().ToString());
    nlohmann::json node_json;
    node_json["id"] = node.id;
    node_json["properties"] = nlohmann::json::parse(node.properties);  // 解析存储的JSON字符串
    
    // 收集邻居信息
    nlohmann::json neighbors = nlohmann::json::array();
    for (const auto& [neighbor_id, weight] : node.neighbors) {
      nlohmann::json neighbor;
      neighbor["id"] = neighbor_id;
      neighbor["weight"] = weight;
      neighbors.push_back(neighbor);
    }
    node_json["neighbors"] = neighbors;
    
    nodes.push_back(node_json);
  }
  result["nodes"] = nodes;
  
  // 收集所有边的属性
  nlohmann::json edges = nlohmann::json::array();
  for (it->Seek(kEdgeKeyPrefix); it->Valid(); it->Next()) {
    std::string key = it->key().ToString();
    if (key.compare(0, strlen(kEdgeKeyPrefix), kEdgeKeyPrefix) != 0) {
      break;
    }
    
    // 解析边的key (格式: "e:from_id:to_id")
    std::string edge_info = key.substr(strlen(kEdgeKeyPrefix));
    size_t delimiter_pos = edge_info.find(':');
    NodeId from_id = std::stoll(edge_info.substr(0, delimiter_pos));
    NodeId to_id = std::stoll(edge_info.substr(delimiter_pos + 1));
    
    nlohmann::json edge;
    edge["from"] = from_id;
    edge["to"] = to_id;
    edge["properties"] = nlohmann::json::parse(it->value().ToString());  // 解析存储的JSON字符串
    
    edges.push_back(edge);
  }
  result["edges"] = edges;
  
  return result;
}

} // namespace vgraph
