# VGraph 图数据库

## 项目概述
- 项目名称: vgraph
- 基于leveldb实现的图数据库存储引擎
- 使用http协议提供接口
- 使用C++17编写
- 提供Web可视化界面

![VGraph Web界面](images/vgraph_web.png)

## 功能特性
- 支持添加节点和边
- 支持查询两点间最短路径
- 支持查询节点的N度邻居
- 使用leveldb作为底层存储引擎
- 提供Web可视化界面
- 支持错误处理和异常情况
- 完整的日志记录

## 目录结构
```
.
├── src/                    # 源代码目录
│   ├── common/            # 公共头文件
│   ├── graphdb/           # 图数据库核心实现
│   ├── main/              # 主程序
│   ├── test/              # 测试用例
│   └── util/              # 工具类
├── third_party/           # 第三方库
├── web/                   # Web界面
├── output/                # 编译输出
└── docs/                  # 文档
```

## 快速开始

### 编译
```bash
# 编译主程序
make main

# 编译并运行测试
make test
make runtest

# 生成测试覆盖率报告
make coverage
```

### 启动服务
1. 启动后端服务
```bash
./output/main/vgraph_server
```

2. 启动Web服务
```bash
cd web
pip install -r requirements.txt
python app.py
```

3. 访问Web界面
打开浏览器访问 `http://localhost:5000`

### 使用示例

#### 1. 添加节点
```bash
# 添加城市节点
curl -X POST http://127.0.0.1:8080/node \
  -H "Content-Type: application/json" \
  -d '{"id": 1, "properties": {"name": "北京"}}'
```

#### 2. 添加边
```bash
# 添加城市间的高铁连接
curl -X POST http://127.0.0.1:8080/edge \
  -H "Content-Type: application/json" \
  -d '{
    "from": 1,
    "to": 2,
    "weight": 4.5,
    "properties": {"type": "高铁"}
  }'
```

#### 3. 查询最短路径
```bash
# 查询从北京到深圳的最短路径
curl http://127.0.0.1:8080/shortest_path/1/4
```

## Web界面功能
- 添加节点：输入节点ID和名称
- 添加边：指定起始节点、目标节点、权重和类型
- 查找最短路径：选择起始和目标节点
- 可视化展示：
  - 动态力导向图布局
  - 节点可拖拽
  - 显示边的权重
  - 最短路径高亮显示

## 示例数据
项目提供了一个旅游信息图数据库示例，包含：
- 5个城市节点 (北京、上海、广州、深圳、杭州)
- 5个景点节点 (故宫、外滩、西湖等)
- 5个餐厅节点 (全聚德、南翔小笼等)
- 5个酒店节点 (北京饭店、和平饭店等)
- 城市之间的高铁连接
- 城市与景点、餐厅、酒店的连接

## 依赖的第三方库
- googletest (1.14.0): C++测试框架
- cpp-httplib: HTTP服务器库
- spdlog (v1.13.0): 日志库
- nlohmann_json (v3.10.0): JSON处理库
- leveldb (1.22): 键值存储库
- cxxopts (v3.2.0): 命令行参数解析库

## 开发规范
- 遵循Google C++代码风格
- 使用现代C++11/14/17特性
- 完整的错误处理
- 英文注释
- 单一职责原则
- 代码整洁可读

## 许可证
本项目采用MIT许可证
