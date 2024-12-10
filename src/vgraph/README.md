# VGraph Graph Database

## Overview
- Project Name: vgraph
- A graph database storage engine based on leveldb
- HTTP protocol interface
- Written in C++17
- Web visualization interface

![VGraph Web Interface](images/vgraph_web.png)

## Features
- Support adding nodes and edges
- Support shortest path queries between two points
- Support N-degree neighbor queries
- Using leveldb as underlying storage engine
- Web visualization interface
- Error handling and exception cases
- Complete logging system

## Directory Structure
```
.
├── src/                    # Source code directory
│   ├── common/            # Common header files
│   ├── graphdb/           # Graph database core implementation
│   ├── main/              # Main program
│   ├── test/              # Test cases
│   └── util/              # Utility classes
├── third_party/           # Third-party libraries
├── web/                   # Web interface
├── output/                # Compilation output
└── docs/                  # Documentation
```

## Quick Start

### Compilation
```bash
# Compile main program
make main

# Compile and run tests
make test
make runtest

# Generate test coverage report
make coverage
```

### Start Services
1. Start backend service
```bash
./output/main/vgraph_server
```

2. Start web service
```bash
cd web
pip install -r requirements.txt
python app.py
```

3. Access web interface
Open browser and visit `http://localhost:5000`

### Usage Examples

#### 1. Add Node
```bash
# Add a city node
curl -X POST http://127.0.0.1:8080/node \
  -H "Content-Type: application/json" \
  -d '{"id": 1, "properties": {"name": "Beijing"}}'
```

#### 2. Add Edge
```bash
# Add high-speed rail connection between cities
curl -X POST http://127.0.0.1:8080/edge \
  -H "Content-Type: application/json" \
  -d '{
    "from": 1,
    "to": 2,
    "weight": 4.5,
    "properties": {"type": "high-speed-rail"}
  }'
```

#### 3. Query Shortest Path
```bash
# Query shortest path from Beijing to Shenzhen
curl http://127.0.0.1:8080/shortest_path/1/4
```

## Web Interface Features
- Add nodes: Input node ID and name
- Add edges: Specify source node, target node, weight and type
- Find shortest path: Select source and target nodes
- Visualization:
  - Dynamic force-directed graph layout
  - Draggable nodes
  - Display edge weights
  - Highlight shortest path

## Sample Data
The project provides a tourism information graph database example, including:
- 5 city nodes (Beijing, Shanghai, Guangzhou, Shenzhen, Hangzhou)
- 5 attraction nodes (Forbidden City, The Bund, West Lake, etc.)
- 5 restaurant nodes (Quanjude, Nanxiang Steamed Buns, etc.)
- 5 hotel nodes (Beijing Hotel, Peace Hotel, etc.)
- High-speed rail connections between cities
- Connections between cities and attractions, restaurants, hotels

## Third-Party Libraries
- googletest (1.14.0): C++ testing framework
- cpp-httplib: HTTP server library
- spdlog (v1.13.0): Logging library
- nlohmann_json (v3.10.0): JSON processing library
- leveldb (1.22): Key-value storage library
- cxxopts (v3.2.0): Command line argument parsing library

## Development Guidelines
- Follow Google C++ Style Guide
- Use modern C++11/14/17 features
- Complete error handling
- English comments
- Single responsibility principle
- Clean and readable code

## License
This project is licensed under the MIT License
