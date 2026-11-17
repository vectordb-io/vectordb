# 分布式 M2 运行与验证

`vectordb-cluster` 实现了设计中的集合级日志读写与共享恢复链路：独立 writer、多个完整 query 副本、独立 checkpoint 任务，连接真实 etcd、Kafka 和 S3。当前为实验性 M2 运行时，尚未完成分片查询和生产高可用；不能替代完整的 Milvus 集群。

后续范围、协议和实施顺序以 [Rust 核心架构与实施计划](implementation-plan.md) 为准。本文仅记录运行与验证方法；唯一实施计划仍为上述文档。

## 已实现的契约

- 每个进程绑定一个 cluster、collection、generation，数据库固定为 default。同一 generation 只能有一个 writer。
- etcd 30 秒所有权租约，每 5 秒续租；Kafka 固定 transactional.id 隔离旧 producer。失去所有权或提交结果不确定后，writer 停止服务，必须重启恢复。
- mutation 通过原生主键状态表和批次暂存校验，以一个 Kafka 事务写入请求、幂等键、摘要和结果。提交后才修改可见状态并返回。批次失败不污染原状态或消耗自动 ID。
- 幂等结果随日志和检查点恢复；同一键不同内容返回 409。键格式为 `v2-<issuedUnixMs>-<nonce>`，24 小时到期，时间最多允许领先 60 秒。超时或 503 后在有效期内使用原键重试；过期返回 409，先核对结果，不得换时间戳重发同一未知结果的请求。
- query 先向 writer 获取已提交 barrier，再回放到该位置并固定读视图，释放集合锁后并行执行查询。各 query 是完整集合副本，可以独立运行在不同主机；它们尚不拆分集合数据。
- checkpoint 先上传不可变对象、下载校验，再通过 etcd CAS 发布 manifest。重启从共享检查点与 Kafka 尾部恢复，无需计算节点本地持久卷。
- readiness 会执行完整读屏障；healthz 仅表示 HTTP 进程存活。所有数据请求要求 Bearer Token，写请求还要求 Idempotency-Key。

## 与目标设计的阶段性差异

当前集群使用原生段引擎，检查点为当前状态的 Parquet 标量列/主键版本列及二进制向量文件；新集合 dense 默认 Float32，`vectorDtype: "Float64"` 可显式保留双精度。writer 每 128 条提交记录同步检查点化，发布成功后截断 Kafka 前缀，并通过 etcd pin 协议回收旧检查点对象；落后副本重新加载状态。没有整库复制和 48 MiB 累计历史上限。

当前行状态的编码与行开销预算为 256 MiB，集合元数据 1 MiB，24 小时幂等结果 16 MiB；请求体上限 512 KiB，日志记录 1 MiB，单对象 64 MiB，每进程最多 32 个运行或排队请求。M2 另有索引缓存 256 MiB、构建 256 MiB、查询工作区 128 MiB、读视图 2 GiB 的计费准入，超限返回 503 或回退 FLAT。这些不等于 RSS 上限，seal 仍有额外缓冲。记录未过期时幂等额度耗尽会拒绝新请求。

查询仍为完整集合副本；HNSW/IVF_FLAT 和标量索引按段持久化，重启直接加载，后台补建缺失或落后的桶。新写入走精确增量，查询不触发全集合索引重建。固定读视图支持独立查询并行。内部运行通信仍是受认证 HTTP；gRPC/mTLS、分片、自动 Coordinator 和生产 HA 尚未完成。上传后未发布即崩溃的孤儿对象保守保留，后台孤儿 GC 和完整备份迁移属于 M6。

支持 create collection、insert、upsert、显式 `entities/delete`（`ids` 数组）、create partition、create/drop index，以及 describe、stats、get、query、search 和索引/分区查询。过滤删除、drop/recreate、Schema 演进、别名、混合检索等未开放操作明确拒绝。通过 `/capabilities` 查看当前模式。

**格式升级：** M2 读取 M1 状态格式 2，写入带索引引用的状态格式 3；raw 段和 WAL 保持格式 2。采用停机升级所有计算进程，禁止 M1/M2 混跑和直接降级。旧 P1 格式 1 仍需使用旧 reader 按 M6 的离线迁移流程处理。

## 构建

需要 Rust 1.89 或更新版本、C/C++ 编译器、CMake 与 zlib 开发库，librdkafka 随 crate 构建。Ubuntu 可安装 `cmake build-essential pkg-config zlib1g-dev`；macOS 使用 Xcode Command Line Tools 和 `brew install cmake`。

```sh
cargo build -p vectordb-cluster --locked
cargo test --workspace --locked
```

单机默认构建入口仍为 `cargo build --locked`，单机 HTTP 和 Python API 保留原有行为。

## 本地依赖

```sh
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml up -d
```

Compose 只绑定本机回环地址，使用单节点开发配置与公开测试凭据。etcd 为 12379，Kafka 为 19092，S3 为 19000。Kafka 单副本只能验证恢复流程，不能证明生产副本容灾能力。

MinIO 社区版已改为源码分发，原镜像仓库可能无法拉取。[MinIO 上游说明](https://github.com/minio/minio)。开发 Compose 使用固定版本的 [Silo MinIO 分支](https://github.com/pgsty/silo/releases/tag/RELEASE.2026-09-16T00-00-00Z) 提供 S3 服务；可替换为自己的 MinIO 或 S3。当前 S3 适配器使用 SigV4、静态 access/secret key、路径寻址，尚不支持 IAM 临时凭据和 session token。

以下命令创建示例 generation 的专用单分区 topic 和对象桶；不要增加 topic 分区数。

```sh
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml exec -T kafka \
  /opt/kafka/bin/kafka-topics.sh --bootstrap-server kafka:9094 --create --if-not-exists \
  --topic vdb.development.items.g2 --partitions 1 --replication-factor 1 \
  --config retention.ms=-1 --config retention.bytes=-1 --config max.message.bytes=2097152

docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml exec -T minio \
  mcli alias set dev http://localhost:9000 development development-only-password
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml exec -T minio \
  mcli mb --ignore-existing dev/vectordb
```

## 运行计算进程

每个终端设置相同环境；以下凭据只用于本地示例。

```sh
export VECTORDB_CLUSTER=development
export VECTORDB_COLLECTION=items
export VECTORDB_GENERATION=g2
export VECTORDB_ETCD=http://127.0.0.1:12379
export VECTORDB_KAFKA=127.0.0.1:19092
export VECTORDB_S3_ENDPOINT=http://127.0.0.1:19000
export VECTORDB_S3_ACCESS_KEY=development
export VECTORDB_S3_SECRET_KEY=development-only-password
export VECTORDB_CLUSTER_TOKEN=development-integration-token
```

写入进程：

```sh
./target/debug/vectordb-cluster --role writer \
  --bind 127.0.0.1:19531 --advertise http://127.0.0.1:19531
```

独立查询进程，同时转发写请求到当前 owner：

```sh
./target/debug/vectordb-cluster --role query --bind 127.0.0.1:19532
```

同一个 query 可以继续添加更多完整副本。跨机器运行时，advertise 必须能从查询节点访问，Kafka advertised listeners 也必须使用可达地址。当前依赖适配器尚无完整的认证与加密配置，只在可信隔离网络中评估。

```sh
# 生成一次后保留；重试时保持整个键不变，不重新计算时间戳。
ISSUED_MS=$(python3 -c 'import time; print(int(time.time()*1000))')
CREATE_KEY="v2-$ISSUED_MS-create-items"
INSERT_KEY="v2-$ISSUED_MS-batch-1"

curl --fail-with-body http://127.0.0.1:19532/v2/vectordb/collections/create \
  -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN" \
  -H "Idempotency-Key: $CREATE_KEY" -H 'Content-Type: application/json' \
  -d '{"collectionName":"items","dimension":2,"autoID":true}'

curl --fail-with-body http://127.0.0.1:19532/v2/vectordb/entities/insert \
  -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN" \
  -H "Idempotency-Key: $INSERT_KEY" -H 'Content-Type: application/json' \
  -d '{"collectionName":"items","data":[{"vector":[1,0]}]}'

curl --fail-with-body http://127.0.0.1:19532/v2/vectordb/entities/search \
  -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"items","data":[[1,0]],"limit":1}'

./target/debug/vectordb-cluster --role checkpoint
```

正常退出 writer 会撤销租约；SIGKILL 后需等待最多约 30 秒租约到期再启动替代 writer。替代进程会建立新 epoch，解决旧 Kafka 事务，重建状态后才监听 HTTP。不能在失败进程中重新初始化 producer。

## 索引任务和状态

以下操作作用于请求到达的节点，都是派生索引管理，不改变已确认实体，也不要求 Idempotency-Key。请求体为 `{"collectionName":"items"}`，仍需 Bearer Token：

- `POST /v2/vectordb/indexes/build`：请求构建或补建全部本地桶。
- `POST /v2/vectordb/indexes/get_status`：查看任务、缓存计费、已加载/已构建桶数、裁剪和回退计数。
- `POST /v2/vectordb/indexes/cancel`：协作取消运行中的构建，并暂停未调度的桶；查询继续使用已有索引或 FLAT。
- `POST /v2/vectordb/indexes/retry`：恢复调度，重试失败或阻塞的桶。

后台最多两个构建任务，默认自动重建阈值为一个桶内 256 个变更的存活 PK。成功的索引随下一次 checkpoint 上传并发布；失败不会发布部分图。控制请求的成功表示已接受调度，可通过 get_status 等待 ready。任务状态本身在当前进程内，跨进程调度属于 M5。

## 真实依赖测试

```sh
cargo build -p vectordb-cluster --locked
python3 scripts/cluster_smoke.py
cargo test -p vectordb-cluster --test backends --locked -- --ignored
```

脚本使用独立随机 generation，启动真实本机进程并连接 Compose 服务，覆盖：重复请求与冲突、失败批次回滚、自动 ID 连续性、并发写、两个查询副本、原生段与 S3 状态检查点、upsert/delete、自动 flush/WAL 截断/旧对象 GC、落后 query 重载、所有计算进程 SIGKILL 并删除测试日志中已检查点化的前缀后恢复、拒绝第二个 writer、旧 writer 暂停到租约过期并接管后的隔离，以及 Kafka 不可用时拒绝确认新写入。脚本会短暂停止测试项目的 Kafka，结束时恢复它。

脚本删除自己的进程和 topic；测试 catalog 和对象保留在专用 Compose 卷内，可通过以下命令清理全部测试依赖。不要对业务部署执行此命令。

```sh
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml down -v
```

尚未验证单故障域丢失、etcd 多数派丢失、持续网络分区、生产吞吐、百万级数据、跨区域恢复和完整多角色滚动升级；这些按 [统一实施计划](implementation-plan.md) 的 M4—M6 及后续扩展分别验收。

## M0—M2 验收入口

类型、物理格式、错误语义、幂等期限、资源预算、回收协议和测试证据统一记录在 [实施计划第 12 节](implementation-plan.md)。原生状态基准见 [m1-baseline.json](benchmarks/m1-baseline.json)，索引 recall/延迟/内存基准见 [m2-baseline.json](benchmarks/m2-baseline.json)。CI 运行 workspace 测试、最低 Rust 版本、Clippy 和真实依赖测试。

这些结果验证 M0—M2 的契约、可持续恢复和查询执行；M3—M6 及生产 HA 尚未完成。
