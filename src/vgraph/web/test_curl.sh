# 添加节点1-5 (城市)
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 1, "properties": {"name": "北京"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 2, "properties": {"name": "上海"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 3, "properties": {"name": "广州"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 4, "properties": {"name": "深圳"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 5, "properties": {"name": "杭州"}}'

# 添加节点6-10 (景点)
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 6, "properties": {"name": "故宫"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 7, "properties": {"name": "外滩"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 8, "properties": {"name": "西湖"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 9, "properties": {"name": "珠峰塔"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 10, "properties": {"name": "世界之窗"}}'

# 添加节点11-15 (餐厅)
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 11, "properties": {"name": "全聚德"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 12, "properties": {"name": "南翔小笼"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 13, "properties": {"name": "陶然居"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 14, "properties": {"name": "绿茶餐厅"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 15, "properties": {"name": "顺记冰室"}}'

# 添加节点16-20 (酒店)
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 16, "properties": {"name": "北京饭店"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 17, "properties": {"name": "和平饭店"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 18, "properties": {"name": "白天鹅宾馆"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 19, "properties": {"name": "深圳华侨城"}}'
curl -X POST http://127.0.0.1:8080/node -H "Content-Type: application/json" -d '{"id": 20, "properties": {"name": "西子湖四季"}}'
```

### 添加边 (城市之间的交通连接)

# 城市之间的高铁连接
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 1, "to": 2, "weight": 4.5, "properties": {"type": "高铁"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 2, "to": 5, "weight": 2.5, "properties": {"type": "高铁"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 2, "to": 3, "weight": 8.0, "properties": {"type": "高铁"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 3, "to": 4, "weight": 1.0, "properties": {"type": "高铁"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 5, "to": 3, "weight": 6.0, "properties": {"type": "高铁"}}'
```

### 添加边 (城市与景点的连接)

# 城市与景点的连接
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 1, "to": 6, "weight": 1.0, "properties": {"type": "景点"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 2, "to": 7, "weight": 1.0, "properties": {"type": "景点"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 5, "to": 8, "weight": 1.0, "properties": {"type": "景点"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 3, "to": 9, "weight": 1.0, "properties": {"type": "景点"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 4, "to": 10, "weight": 1.0, "properties": {"type": "景点"}}'
```

### 添加边 (城市与餐厅的连接)

# 城市与餐厅的连接
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 1, "to": 11, "weight": 1.0, "properties": {"type": "餐厅"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 2, "to": 12, "weight": 1.0, "properties": {"type": "餐厅"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 3, "to": 13, "weight": 1.0, "properties": {"type": "餐厅"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 5, "to": 14, "weight": 1.0, "properties": {"type": "餐厅"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 4, "to": 15, "weight": 1.0, "properties": {"type": "餐厅"}}'
```

### 添加边 (城市与酒店的连接)

# 城市与酒店的连接
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 1, "to": 16, "weight": 1.0, "properties": {"type": "酒店"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 2, "to": 17, "weight": 1.0, "properties": {"type": "酒店"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 3, "to": 18, "weight": 1.0, "properties": {"type": "酒店"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 4, "to": 19, "weight": 1.0, "properties": {"type": "酒店"}}'
curl -X POST http://127.0.0.1:8080/edge -H "Content-Type: application/json" -d '{"from": 5, "to": 20, "weight": 1.0, "properties": {"type": "酒店"}}'