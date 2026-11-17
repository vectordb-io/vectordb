# 分布式 M4 运行与验证

`vectordb-cluster --role node` 提供多集合、命名空间、别名和可恢复的数据操作，连接真实 etcd、Kafka 与 S3。`query-node` 支持只加载分配的分片、查询副本和全局结果合并。当前仍是实验性运行时，自动调度和生产高可用尚待 M5—M6。

本文是运行说明，架构、协议和验收依据统一在 [implementation-plan.md](implementation-plan.md)。文件名沿用早期 P1 路径，以保留已有链接。

## 数据与恢复契约

- 名字在 etcd 目录解析为不可复用的 collection_id/generation。创建、删除中途崩溃后，node 恢复未完成意图；同名重建使用新身份，不接收旧日志。
- 同一身份只有一个 writer，etcd 租约为 30 秒、每 5 秒续租。接管必须完成 Assigning、Recovering、Kafka fencing、激活记录回放和 Active 发布；失败的旧任务不能重新初始化 producer。
- mutation 的请求、结果和自动 ID 在一个 Kafka 事务提交，提交后才返回成功。批量校验失败不修改数据、不消耗自动 ID。分区删除是有序、原子的控制操作；默认分区不能删除。
- 写请求携带 `Idempotency-Key: v2-<issuedUnixMs>-<nonce>`，有效期 24 小时。重试保持整个键、请求体和访问名字不变。相同键不同内容返回 409，过期明确拒绝。
- 路由凭证在发送写入前固定目标身份。别名切换或同名重建后，原请求不会误投新集合；已完成实体请求返回旧结果，未执行且目标已删除的请求明确失败。
- Strong 读取先取得 writer 屏障，回放到对应位置，再固定不可变视图并释放写锁执行查询。没有跨集合事务或跨集合快照。
- writer 每 128 条记录自动检查点化；`collections/flush` 可显式发布检查点。Parquet、向量段及 HNSW/IVF_FLAT 索引上传 S3 并校验后，再 CAS 发布 manifest。恢复只需要共享检查点与 WAL 尾部。
- 所有数据和内部控制请求需要 Bearer Token。node 的 `/readyz` 验证目录可达，具体集合请求独立检查 owner 与读屏障；它不表示每个集合都已预热。`/healthz` 只表示进程存活。

## 构建与依赖

需要 Rust 1.89 或更新版本、C/C++ 编译器、CMake 与 zlib 开发库。Ubuntu 可安装 `cmake build-essential pkg-config zlib1g-dev`；macOS 使用 Xcode Command Line Tools 和 CMake。

```sh
cargo build -p vectordb-cluster --locked
cargo test --workspace --locked
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml up -d

docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml exec -T minio \
  mcli alias set dev http://localhost:9000 development development-only-password
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml exec -T minio \
  mcli mb --ignore-existing dev/vectordb
```

Compose 仅绑定回环地址，使用单节点开发配置和公开测试凭据：etcd 12379、Kafka 19092、S3 19000。S3 服务使用固定版本的 Silo MinIO 分支；可替换为自己的 S3 服务。适配器使用 SigV4、静态 access/secret key、路径寻址，暂不支持 IAM 临时凭据。

node 自动创建每个集合 generation 的专用单分区 Kafka topic，关闭时间/容量自动过期，由检查点协议安全截断；不要手动增加分区数。`--wal-replicas` 默认 1，仅适用于开发验证，可按 broker 数量调整。该开发配置不能证明生产容灾能力。

## 运行多集合节点

每个终端设置相同的环境，下面凭据仅用于本地测试：

```sh
export VECTORDB_CLUSTER=development
export VECTORDB_ETCD=http://127.0.0.1:12379
export VECTORDB_KAFKA=127.0.0.1:19092
export VECTORDB_S3_ENDPOINT=http://127.0.0.1:19000
export VECTORDB_S3_ACCESS_KEY=development
export VECTORDB_S3_SECRET_KEY=development-only-password
export VECTORDB_CLUSTER_TOKEN=development-integration-token

# 终端一
./target/debug/vectordb-cluster --role node \
  --bind 127.0.0.1:19531 --advertise http://127.0.0.1:19531

# 终端二，使用相同环境
./target/debug/vectordb-cluster --role node \
  --bind 127.0.0.1:19532 --advertise http://127.0.0.1:19532
```

无需指定 collection 或 generation。任意节点可接收请求，按集合获取 writer 归属，或者向现有 owner 转发写入并在本节点执行查询。正常退出会撤销租约；崩溃后，待旧租约到期，后续请求可驱动新 assignment 恢复，不需要修改 etcd 元数据。跨机器运行时，advertise 和 Kafka advertised listeners 都必须可达；当前内部通信只适合可信隔离网络，mTLS 和故障域调度仍属于 M5。

```sh
# 每个新操作生成一个键；重试复用原键。
ISSUED_MS=$(python3 -c 'import time; print(int(time.time()*1000))')
CREATE_KEY="v2-$ISSUED_MS-create-items"
INSERT_KEY="v2-$ISSUED_MS-batch-1"

curl --fail-with-body http://127.0.0.1:19531/v2/vectordb/collections/create \
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
```

## 分片查询与副本

使用上面的相同环境，再启动两个专用查询节点：

```sh
# 终端三
./target/debug/vectordb-cluster --role query-node \
  --bind 127.0.0.1:19533 --advertise http://127.0.0.1:19533
# 终端四
./target/debug/vectordb-cluster --role query-node \
  --bind 127.0.0.1:19534 --advertise http://127.0.0.1:19534
```

新建 node 集合默认 16 个分片；创建请求可指定 `shardCount` 为 1/2/4/8/16/32，之后不能修改。下面把默认 16 分片的 items 前 8 个交给 19533，后 8 个交给 19534：

```sh
python3 - <<'PYROUTES' > /tmp/vectordb-routes.json
import json
print(json.dumps({"collectionName":"items", "expectedEpoch":0,
                  "replicas":[["http://127.0.0.1:19533"]]*8 + [["http://127.0.0.1:19534"]]*8}))
PYROUTES
ROUTE_MS=$(python3 -c 'import time; print(int(time.time()*1000))')
curl --fail-with-body http://127.0.0.1:19531/v2/vectordb/collections/set_query_routes \
  -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN" \
  -H "Idempotency-Key: v2-$ROUTE_MS-route-items" -H 'Content-Type: application/json' \
  --data-binary @/tmp/vectordb-routes.json
```

`replicas[i]` 列出分片 i 的 1—3 个 HTTP(S) origin，按顺序选择并故障重试。数组必须覆盖全部分片；更新时传当前 `expectedEpoch`，CAS 冲突返回 409。用 `collections/get_query_routes` 查看当前映射，用 `collections/shard_status` 在各 Query 节点查看已加载分片、行数、状态字节、水位及 WAL 回放记录数。两者请求体均为 `{"collectionName":"items"}`。修改地址列表即可手动迁移，新的分配在首个查询时从 S3/WAL 加载；自动发现、故障域放置、主动预热与再平衡由 M5 实现。

发布路由后，向 node 或 query-node 发起实体读请求，代理在同一个 B 上收齐所有分片再合并。响应带 `readToken` 和 `routeEpoch`。query-node 不接受写入、不能接管 writer，也不加载完整集合；写请求仍发给 node。没有路由的集合保留原完整副本读取，writer 也仍保留完整状态。副本数量增加会相应增加 Query 的持久数据份数及 WAL 读取量；同一节点的多个分片共享一次 WAL 回放，当前按查询追赶。

读请求可加 `timeoutMs`（1—30000，默认 30000）。主副本失败可在同一 B 上切备副本；检查点失效/迁移可整次重试新 B。缺少分片或超时返回错误，不返回部分成功。每次请求的 offset 分页只对应此次快照，多次 HTTP 请求不保证同一快照。全局 `limit + offset` 上限 16384，混合子检索最多 16 个，先合并各子检索全局排名再融合。代理所有尝试累计响应预算 32 MiB；worker 单响应同样限 32 MiB。HTTP 超时后，已发出的阻塞后端调用仍需到自身超时才退出。

专用 Query 每集合负责的数据状态上限 256 MiB；索引、工作区、构建与读视图预算按集合总分片数分摊。迁移淘汰旧缓存，但在途不可变视图和有限租约 pin 保护旧快照。该限制是计费准入，不等同进程 RSS 硬上限。

复现固定负载的 1→2 Query 测量（与其他集群测试顺序执行）：

```sh
cargo build --release -p vectordb-cluster --locked
VECTORDB_M4_BINARY="$PWD/target/release/vectordb-cluster" \
  python3 -u scripts/cluster_m4_smoke.py --benchmark
```

这会写入 `docs/benchmarks/m4-baseline.json`。10k × 32 Float32、16 分片、FLAT top-10、每请求 16 个向量、并发 1，记录精确结果比对、吞吐、延迟、分配行数/字节及运行后 RSS 样本；不代表跨主机扩容验收。故障冒烟使用 debug 构建，`VECTORDB_TEST_SHARD_DELAY_MS` 仅在 debug 下启用。

## API 与资源边界

`GET /capabilities` 的 `operations` 是实际开放清单，未开放操作返回 501。Schema 创建后固定；过滤删除、Schema 演进、rename/truncate、分组扩展和 Session/Bounded/Eventual 尚未在集群开放。

| 操作 | 请求要点 |
| --- | --- |
| `databases/create/list/describe/drop` | `dbName`；只能删除空的非 default 命名空间 |
| `collections/create/drop/list/has/describe/get_stats` | `dbName` 可省略为 default；drop 使用规范集合名 |
| `aliases/create/alter/drop/list/describe` | `aliasName`；create/alter 的 collectionName 必须是规范集合名 |
| `entities/insert/upsert/delete` | delete 使用 `ids` 数组；批量全成或全败 |
| `entities/get/query/count/search/hybrid_search` | count 支持 filter/partitionNames，返回 `{count:N}` |
| `partitions/create/drop/list/has/get_stats` | drop 使用非空 `partitionNames` 数组 |
| `indexes/create/drop/list/describe` | 索引配置通过 WAL 提交并恢复 |
| `collections/flush` | 发布该集合检查点，不要求幂等键 |
| `indexes/build/retry/cancel/get_status` | 作用于接收请求的节点；不要求幂等键，仍需认证 |

命名空间及路由名字限 1..64 个 ASCII 字母、数字、`_`、`-`。集合定义至多 64 KiB，目录至多 1 MiB、32 个命名空间、16 个活跃集合、256 个保留身份；暂不自动回收旧 generation 的 topic/对象和身份，达到身份额度后拒绝新建，后续由 M6 完成安全回收。每 node 最多缓存 16 个集合、32 个运行或排队请求、两个索引 worker。集群路由凭证最多 32,768 个，在 24 小时有效期后加约一分钟保守余量，由后续新请求原子推进单调时钟并清理旧记录、释放预算；已有请求的重试不新增额度。达到目录/凭证额度时明确拒绝新请求，不驱逐仍有效的绑定。

每集合保留原有预算：当前状态 256 MiB、元数据 1 MiB、幂等结果 16 MiB、索引缓存 256 MiB、构建 256 MiB、查询工作区 128 MiB、读视图 2 GiB。HTTP 请求体 512 KiB、WAL 记录 1 MiB、单对象 64 MiB。这些是计费准入，不能视为进程 RSS 的硬上限；多个集合的内存占用会累加。

单机 HTTP/Python 仍使用 SQLite。旧 `writer/query/checkpoint` 角色保留用于单集合回归测试，需要显式 collection/generation 及预建 topic；node 不会自动收编这类旧集合。raw/WAL 格式仍为 2，分片集合使用状态 manifest 4，单分片仍写 3；读取兼容 manifest 2/3（按单分片处理）。目录格式为 1，新增可选分片与路由字段。已有集合不自动改变分片数；写出 manifest 4 后，旧二进制无法读取该集合检查点。正式迁移工具属于 M6。

## 测试入口

依赖启动并建好对象桶后执行：

```sh
cargo test --workspace --locked
cargo +1.89 test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
python3 scripts/cluster_smoke.py
python3 -u scripts/cluster_m3_smoke.py
python3 -u scripts/cluster_m4_smoke.py
cargo test -p vectordb-cluster --test backends --locked -- --ignored
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml down -v
```

三份集群脚本按顺序运行：旧脚本会暂停 Kafka 来验证故障恢复。M3 脚本覆盖双节点操作和 12 个崩溃场景，包括已提交但未回复、接管阶段、集合生命周期，以及路由绑定后崩溃再切换别名；还验证同一节点进程内从未知提交结果恢复。调试构建的 `VECTORDB_TEST_CRASH_AT` 仅供测试，release 构建不执行该钩子。

契约和验证记录见 [统一实施计划](implementation-plan.md)，性能历史记录见 [M1 状态基准](benchmarks/m1-baseline.json)、[M2 索引基准](benchmarks/m2-baseline.json) 和 [M4 查询扩展基准](benchmarks/m4-baseline.json)。M4 未宣称完成跨主机故障域、生产吞吐、百万级数据、72 小时稳定性、备份迁移或核心 V1 发布验收。
