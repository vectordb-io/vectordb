#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include "graphdb.h"

using namespace vgraph;
using json = nlohmann::json;

int main(int argc, char** argv) {
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
  spdlog::info("Starting vgraph server...");
  
  // 创建图数据库实例
  std::unique_ptr<GraphDB> db;
  try {
    db = std::make_unique<GraphDB>("/tmp/vgraph_db");
  } catch (const std::exception& e) {
    std::cerr << "Failed to create database: " << e.what() << std::endl;
    return 1;
  }

  // 创建HTTP服务器
  httplib::Server svr;

  // 1. 添加节点
  svr.Post("/node", [&](const httplib::Request& req, httplib::Response& res) {
    try {
      auto j = json::parse(req.body);
      GraphDB::NodeId node_id = j["id"].get<GraphDB::NodeId>();
      std::string properties = j["properties"].dump();
      
      bool success = db->AddNode(node_id, properties);
      if (success) {
        res.status = 200;
        res.set_content("{\"status\": \"success\"}", "application/json");
      } else {
        res.status = 400;
        res.set_content("{\"status\": \"error\", \"message\": \"Node already exists\"}", 
                       "application/json");
      }
    } catch (const std::exception& e) {
      res.status = 400;
      json error = {{"status", "error"}, {"message", e.what()}};
      res.set_content(error.dump(), "application/json");
    }
  });

  // 2. 添加边
  svr.Post("/edge", [&](const httplib::Request& req, httplib::Response& res) {
    try {
      auto j = json::parse(req.body);
      GraphDB::NodeId from_id = j["from"].get<GraphDB::NodeId>();
      GraphDB::NodeId to_id = j["to"].get<GraphDB::NodeId>();
      GraphDB::Weight weight = j["weight"].get<GraphDB::Weight>();
      std::string properties = j["properties"].dump();
      
      bool success = db->AddEdge(from_id, to_id, weight, properties);
      if (success) {
        res.status = 200;
        res.set_content("{\"status\": \"success\"}", "application/json");
      } else {
        res.status = 400;
        res.set_content("{\"status\": \"error\", \"message\": \"Invalid nodes\"}", 
                       "application/json");
      }
    } catch (const std::exception& e) {
      res.status = 400;
      json error = {{"status", "error"}, {"message", e.what()}};
      res.set_content(error.dump(), "application/json");
    }
  });

  // 3. 查询最短路径
  svr.Get(R"(/shortest_path/(\d+)/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
    try {
      GraphDB::NodeId from_id = std::stoll(req.matches[1]);
      GraphDB::NodeId to_id = std::stoll(req.matches[2]);
      
      GraphDB::Path path;
      bool found = db->ShortestPath(from_id, to_id, &path);
      
      if (found) {
        json result;
        result["nodes"] = path.nodes;
        result["total_weight"] = path.total_weight;
        res.set_content(result.dump(), "application/json");
      } else {
        res.status = 404;
        res.set_content("{\"status\": \"error\", \"message\": \"Path not found\"}", 
                       "application/json");
      }
    } catch (const std::exception& e) {
      res.status = 400;
      json error = {{"status", "error"}, {"message", e.what()}};
      res.set_content(error.dump(), "application/json");
    }
  });

  // 4. 查询N度邻居
  svr.Get(R"(/neighbors/(\d+)/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
    try {
      GraphDB::NodeId node_id = std::stoll(req.matches[1]);
      int n = std::stoi(req.matches[2]);
      
      auto neighbors = db->GetNeighborsWithinNDegrees(node_id, n);
      json result = {{"neighbors", neighbors}};
      res.set_content(result.dump(), "application/json");
    } catch (const std::exception& e) {
      res.status = 400;
      json error = {{"status", "error"}, {"message", e.what()}};
      res.set_content(error.dump(), "application/json");
    }
  });

  // 5. 获取整个图的JSON表示
  svr.Get("/graph", [&](const httplib::Request& req, httplib::Response& res) {
    try {
      json graph = db->ToJson();
      res.set_content(graph.dump(2), "application/json");
    } catch (const std::exception& e) {
      res.status = 500;
      json error = {{"status", "error"}, {"message", e.what()}};
      res.set_content(error.dump(), "application/json");
    }
  });

  // 启动服务器
  std::cout << "Starting server at http://0.0.0.0:8080" << std::endl;
  svr.listen("0.0.0.0", 8080);

  return 0;
}