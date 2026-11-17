# 分布式 P1 原型运行与验证

`vectordb-cluster` 实现了设计中的集合级日志读写与共享恢复链路：独立 writer、多个完整 query 副本、独立 checkpoint 任务，连接真实 etcd、Kafka 和 S3。它是实验性 P1 原型，尚未完成 P2 分片查询和 P3 生产高可用；不能替代完整的 Milvus 集群。

## 已实现的契约

- 每个进程绑定一个 cluster、collection、generation，数据库固定为 default。同一 generation 只能有一个 writer。
- etcd 30 秒所有权租约，每 5 秒续租；Kafka 固定 transactional.id 隔离旧 producer。失去所有权或提交结果不确定后，writer 停止服务，必须重启恢复。
- mutation 通过内存副本校验，以一个 Kafka 事务写入请求、幂等键、摘要和结果。提交后才修改可见状态并返回。批次失败不污染原状态或消耗自动 ID。
- 幂等结果随日志和检查点恢复；同一键不同内容返回 409。超时或 503 后必须使用原键重试。
- query 先向 writer 获取已提交 barrier，再回放到该位置并在进程锁内执行查询。各 query 是完整集合副本，可以独立运行在不同主机；它们尚不拆分集合数据。
- checkpoint 先上传不可变对象、下载校验，再通过 etcd CAS 发布 manifest。重启从共享检查点与 Kafka 尾部恢复，无需计算节点本地持久卷。
- readiness 会执行完整读屏障；healthz 仅表示 HTTP 进程存活。所有数据请求要求 Bearer Token，写请求还要求 Idempotency-Key。

## 与目标设计的阶段性差异

当前检查点是版本化命令历史，查询执行器使用独立的内存 SQLite Engine；没有上传 SQLite 数据文件，也不共享运行中的数据库文件。下一阶段需要替换为列式段、持久化索引和 MVCC 段视图。

当前内部通信使用受认证 HTTP，尚未实现目标 gRPC/mTLS 协议。Coordinator 自动调度、热备自动接管、分片 fan-out、后台持续检查点、WAL 截断、对象 GC、compaction、完整备份迁移工具和 Helm 生产拓扑尚未实现。Kafka 和对象均保留，不自动删除。

每次校验和回放都会复制内存引擎，历史检查点随操作增长，因此仅用于小数据正确性验证。请求体上限 512 KiB，日志记录 1 MiB，回放历史预算 48 MiB，对象下载上限 64 MiB，每进程最多 32 个运行或排队请求。达到历史预算会拒绝操作，需迁移到新 generation；不能靠重启绕过预算。

支持 create collection、insert、upsert、create partition、create/drop index，以及 describe、stats、get、query、search 和索引/分区查询。删除、drop/recreate、Schema 演进、别名、混合检索等未开放操作明确拒绝。通过 `/capabilities` 查看当前模式。

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
  --topic vdb.development.items.g1 --partitions 1 --replication-factor 1 \
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
export VECTORDB_GENERATION=g1
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
curl --fail-with-body http://127.0.0.1:19532/v2/vectordb/collections/create \
  -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN" \
  -H 'Idempotency-Key: create-items' -H 'Content-Type: application/json' \
  -d '{"collectionName":"items","dimension":2,"autoID":true}'

curl --fail-with-body http://127.0.0.1:19532/v2/vectordb/entities/insert \
  -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN" \
  -H 'Idempotency-Key: batch-1' -H 'Content-Type: application/json' \
  -d '{"collectionName":"items","data":[{"vector":[1,0]}]}'

curl --fail-with-body http://127.0.0.1:19532/v2/vectordb/entities/search \
  -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN" \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"items","data":[[1,0]],"limit":1}'

./target/debug/vectordb-cluster --role checkpoint
```

正常退出 writer 会撤销租约；SIGKILL 后需等待最多约 30 秒租约到期再启动替代 writer。替代进程会建立新 epoch，解决旧 Kafka 事务，重建状态后才监听 HTTP。不能在失败进程中重新初始化 producer。

## 真实依赖测试

```sh
cargo build -p vectordb-cluster --locked
python3 scripts/cluster_smoke.py
```

脚本使用独立随机 generation，启动真实本机进程并连接 Compose 服务，覆盖：重复请求与冲突、失败批次回滚、自动 ID 连续性、并发写、两个查询副本、S3 检查点、所有计算进程 SIGKILL 并删除测试日志中已检查点化的前缀后恢复、拒绝第二个 writer、旧 writer 暂停到租约过期并接管后的隔离，以及 Kafka 不可用时拒绝确认新写入。脚本会短暂停止测试项目的 Kafka，结束时恢复它。

脚本删除自己的进程和 topic；测试 catalog 和对象保留在专用 Compose 卷内，可通过以下命令清理全部测试依赖。不要对业务部署执行此命令。

```sh
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml down -v
```

尚未验证单故障域丢失、etcd 多数派丢失、持续网络分区、生产吞吐、百万级数据、跨区域恢复和完整多角色滚动升级；这些属于设计 P2/P3 的验收要求。

## 2026 年 11 月 17 日验证记录

在 macOS arm64 上通过以下检查：

- Rust stable 和 Rust 1.89 的 workspace 测试：31 个非忽略测试通过。
- `cargo fmt --all --check`、workspace Clippy（warnings as errors）和 workspace 构建通过。
- 真实 etcd 3.5.21 与 Silo 2026-09-16 的租约、单 owner、manifest 单调 CAS、S3 签名读写和禁止覆盖测试通过。
- 真实 Kafka 3.9.1 的完整 `cluster_smoke.py` 通过，包括已检查点化日志前缀的测试性删除、SIGKILL 恢复、暂停旧 writer 后接管与隔离、Kafka 停机拒写。
- 原有 `scripts/smoke.py` 与 `scripts/check_website.py` 通过。

这些结果验证单节点基础设施上的 P1 正确性链路；不等价于生产 HA、网络分区模型证明或性能验收。
