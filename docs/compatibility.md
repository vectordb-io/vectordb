# Milvus 功能对照与路线

参考代码：本工作区的 `reference-code/milvus/`，提交 `61de3a82cce970235f9ffb0c90b38749d0c3b196`。主要核对了 `internal/distributed/proxy/httpserver/handler_v2.go` 的操作列表，以及 `request_v2.go` 的 Schema / entity / search / hybrid / index 请求定义。未修改 Milvus 参考目录，未复制 Go / C++ 实现。

这是独立实现的 **单机 Alpha**，不是完整移植。不能把 REST 路径相似理解为 Milvus 客户端协议兼容，也不能据此声称“大部分 Milvus 功能已经全部完成”。以下清单是后续持续推进的依据。

| 功能族 | Rust 当前状态 | 差异 / 待办 |
| --- | --- | --- |
| 数据库 | 创建、列举、描述、删除 | 无数据库属性 / 配额 |
| 集合 | 创建、描述、has、删除、重命名、truncate、统计 | 无跨数据库重命名、schema 演进、TTL |
| Schema | 显式 Schema、快速创建、动态字段、默认值、nullable | Array 为一般 JSON 数组；不支持 struct、partition key、clustering key |
| 主键 | Int64 / VarChar、Int64 autoID | 无 VarChar 自动 ID；insert 重复主键明确拒绝 |
| 分区 | CRUD、has、统计、load / release | 无 partition key 自动路由 |
| 别名 | create / alter / describe / list / drop | list 要求 collectionName |
| 写入 | 原子批量 insert、完整行 upsert、CLI JSONL 导入导出 | 无部分字段 merge upsert、异步导入任务 |
| 查询 | get、filter、projection、count、offset、标量排序 | 不支持聚合表达式、iterator、表达式模板参数 |
| 稠密向量 | FloatVector、L2 / IP / COSINE | 内部 f64 JSON 表示；无 Float16 / BFloat16 / Int8 向量 |
| 稀疏向量 | SparseFloatVector、精确 IP | 无稀疏倒排索引 / WAND / 自动 BM25 |
| 二进制向量 | BinaryVector、HAMMING / JACCARD | 输入为字节整数数组，不是 Milvus 的所有编码形式 |
| 索引 | FLAT / IVF_FLAT / HNSW | 无 AUTOINDEX 别名、PQ / SQ、DISKANN、GPU；延迟构建和整体重建 |
| 搜索 | batch、filter、分区、范围、单实体分组 | 不支持 groupSize、搜索聚合、复杂排序和 function chains |
| 混合搜索 | 多向量 RRF / weighted | 字段子搜索的显式向量输入；无 embedding function |
| 一致性 | 单进程事务串行、Strong | 无 bounded / session / eventual 和 MVCC 时间旅行 |
| 存储 | SQLite WAL、事务、快照、VACUUM | 无 Milvus segment / binlog 格式，旧数据需应用级迁移 |
| 加载状态 | 集合 / 分区访问状态、释放索引缓存 | 无分布式加载进度、资源组、副本调度 |
| HTTP | `/v2/vectordb` 核心路径与 camelCase | 错误码 / HTTP 状态和部分返回形状不同；多查询增加 queryIndex |
| gRPC / SDK | 未实现 | PyMilvus / Milvus Go / Java SDK 尚不可直接使用 |
| 认证 | 单个 Bearer Token | 无 users / roles / grants / RBAC |
| 运维 | healthz、请求 / 错误计数器、优雅退出 | 无完整 tracing、查询画像、分布式指标 |
| 分布式 | 未实现 | Coordinator、sharding、replication、failover、CDC、消息队列 |
| 存储分离 | 未实现 | S3 / 对象存储、tiering、冷热分层 |
| 文本能力 | 未实现 | tokenizer、全文索引、BM25、文本函数 |
| 生产性能 | 未完成验证 | 仅小数据正确性 / ANN 回归测试，无规模性能承诺 |

## 优先级与验收要求

1. **独立引擎能力与规模**：增量 HNSW / IVF、持久化索引、列式存储、并发查询、资源上限；必须测 recall / latency / memory 与重启一致性。
2. **接口与检索覆盖**：Milvus protobuf / gRPC、SDK 契约测试、BM25 / 稀疏倒排、更多类型、schema 演进、导入导出；必须用真实客户端验证。
3. **权限与运维**：用户 / 角色 / 授权、审计、TLS 接入、指标与恢复工具；必须做拒绝路径和权限隔离测试。
4. **分布式基础设施**：一致性日志、复制、分片路由、元数据协调、对象存储；必须通过节点故障、网络分区、写重试、恢复与一致性测试。

没有实现的操作和不认识的参数返回错误；不能用空响应或伪造的状态替代功能。
