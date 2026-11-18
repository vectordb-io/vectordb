# 分布式运行与验证

M6 原生单机目录、集群权限、维护、迁移、独立恢复与部署见 [M6 运行手册](m6-operations.md)。

`vectordb-cluster --role node` 提供多集合、命名空间、别名和可恢复的数据操作，连接真实 etcd、Kafka 与 S3。`query-node` 支持只加载分配的分片、查询副本和全局结果合并。M5 增加可选的受控模式，支持自动放置、恢复、预热迁移和内部 mTLS/gRPC。当前仍是实验性运行时，真实多主机验收和核心 V1 发布尚未完成。

本文是运行说明，架构、协议和验收依据统一在 [implementation-plan.md](implementation-plan.md)。文件名沿用早期 P1 路径，以保留已有链接。

## 数据与恢复契约

- 名字在 etcd 目录解析为不可复用的 collection_id/generation。创建、删除中途崩溃后，node 恢复未完成意图；同名重建使用新身份，不接收旧日志。
- 同一身份只有一个负责有序提交的 writer，多个 M7 写入网关可以并行准备批次。writer 的 etcd 租约为 30 秒、每 5 秒续租。接管必须完成 Assigning、Recovering、Kafka fencing、激活记录回放和 Active 发布；失败的旧任务不能重新初始化 producer。
- 默认单日志模式下，mutation 的请求、结果和自动 ID 在一个 Kafka 事务提交，提交后才返回成功。批量校验失败不修改数据、不消耗自动 ID。分区删除是有序、原子的控制操作；默认分区不能删除。
- 写请求携带 `Idempotency-Key: v2-<issuedUnixMs>-<nonce>`，有效期 24 小时。重试保持整个键、请求体和访问名字不变。相同键不同内容返回 409，过期明确拒绝。
- 路由凭证在发送写入前固定目标身份。别名切换或同名重建后，原请求不会误投新集合；已完成实体请求返回旧结果，未执行且目标已删除的请求明确失败。
- Strong 读取先取得 writer 屏障，回放到对应位置，再固定不可变视图并释放写锁执行查询。没有跨集合事务或跨集合快照。
- writer 每 128 条记录自动检查点化；`collections/flush` 可显式发布检查点。Parquet、向量段及 HNSW/IVF_FLAT 索引上传 S3 并校验后，再 CAS 发布 manifest。恢复只需要共享检查点与 WAL 尾部。
- 所有数据和内部控制请求需要 Bearer Token。node 的 `/readyz` 验证目录可达，具体集合请求独立检查 owner 与读屏障；它不表示每个集合都已预热。`/healthz` 只表示进程存活。

## 单集合多通道写入（M7）

创建新集合时，可用 `writeChannels` 指定 1、2、4、8 或 16 个 Kafka 准备分区。省略字段时沿用单日志布局。已有集合保持创建时的布局，两种集合可以共用同一个 M7 集群。部署 M7 计算镜像后再创建多通道集合，M6 二进制不支持新增的目录字段、决定格式和检查点截断位置。

```json
{"collectionName":"events","dimension":128,"shardCount":16,"writeChannels":4}
```

向一个或多个 `node` 的原有实体端点发起 insert、upsert、按主键 delete，客户端按请求分摊到写入网关。网关校验实体并向对应 Kafka 分区写入准备数据，等所有分区确认后，才把包含主键、空间估算和准备位置的签名凭证发给集合的决定节点。显式主键按现有哈希路由；自动 ID 在决定节点分配，准备阶段按请求和行序号分散。决定节点校验主键冲突、幂等、分区与配额，以一条决定提交整批。相同键重试可取回原 ID 和 commit token。决定记录只引用准备位置与摘要，每条读取均使用同一个决定日志 offset 作为 Strong 快照。

- `collections/describe` 返回 `writeChannels`。管理员可用 `collections/storage_stats` 查看 `prepareWatermarks`（各分区的 low/high，high 不含在内）和 `prepareFloors`（持久化截断下界）。受控节点的 `/cluster/status` 提供本机累计完成准备的 `local.stagedWriteBatches`，以及决定和完整状态各自的水位。
- 查询副本和接管者从 S3 检查点继续回放决定及所引用的准备数据。缺少分区数据、摘要不符或快照不完整时，请求会失败。准备阶段中断可以使用原幂等键重试，未提交的数据没有可见效果。
- 截断下界由有代际隔离的决定生产者有序写入。每个准备分区预留最近 32 个位置供在途事务使用。后台发布完整检查点并校验成功后，才允许回收更旧的准备数据。已跨过下界的未提交批次返回 503，客户端使用同一幂等键重试。Kafka 的实际磁盘释放还取决于 broker 段滚动和删除延迟。
- 增加写入网关分摊的是请求处理、实体验证及向 Kafka 写入准备数据的负载。每个集合保留一个决定节点处理主键冲突、配额和有序提交，并维护完整检查点。生产容量还受这个节点、etcd 路由凭证和持久化基础设施限制。

本机端到端验证使用 `python3 -u scripts/cluster_m7_smoke.py`，需要 debug 集群二进制和开发 Compose 后端。固定负载写入基准支持原生进程以及带 CPU 配额的 Linux 容器。先按下节启动 Compose 并创建 `vectordb` 桶，再执行以下容器基准。结果与验收范围见[统一计划](implementation-plan.md)。

```sh
docker build -f deployments/cluster/Dockerfile -t vectordb-cluster:m7-local .
python3 scripts/cluster_m7_bench.py --writer-image vectordb-cluster:m7-local --output m7-scaling.json
```

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

node 为每个集合 generation 自动创建单分区的有序日志 topic，启用写通道时另建指定分区数的准备 topic。它们关闭时间和容量自动过期，由检查点协议安全截断。分区数由创建时的布局决定。`--wal-replicas` 默认 1，仅适用于开发验证，可按 broker 数量调整。该开发配置不能证明生产容灾能力。

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

无需指定 collection 或 generation。任意节点可接收请求，按集合获取 writer 归属，或者向现有 owner 转发写入并在本节点执行查询。正常退出会撤销租约；崩溃后，待旧租约到期，后续请求可驱动新 assignment 恢复，不需要修改 etcd 元数据。跨机器运行时，advertise 和 Kafka advertised listeners 都必须可达；当前内部通信只适合可信隔离网络，下面的受控模式提供 mTLS 与故障域调度；本节手动模式仍用于开发/回归。

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

`replicas[i]` 列出分片 i 的 1—3 个 HTTP(S) origin，按顺序选择并故障重试。数组必须覆盖全部分片；更新时传当前 `expectedEpoch`，CAS 冲突返回 409。用 `collections/get_query_routes` 查看当前映射，用 `collections/shard_status` 在各 Query 节点查看已加载分片、行数、状态字节、水位及 WAL 回放记录数。两者请求体均为 `{"collectionName":"items"}`。修改地址列表即可手动迁移，新的分配在首个查询时从 S3/WAL 加载；自动发现、故障域放置、主动预热与再平衡使用下面的 M5 受控模式。

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

## M5 受控集群

本节适用于新建受控集群。前面的手动路由模式继续用于开发和旧测试；两种模式不能混用。若迁移已有手动模式集群，先停止旧计算进程，再部署全部支持 M5 的受控进程；保留 etcd、Kafka、S3 和同一 cluster ID，不需要重建集合。不要在仍运行旧进程时引入 Coordinator。

建议至少两个 Coordinator、两个 node（网关兼 writer）、三个 query-node，并将查询节点放到三个不同的故障域。基础设施本身也需要独立的高可用配置；单节点开发 Compose 不能验证这一点。所有节点继续使用前面的共享存储环境变量，并增加：

```sh
export VECTORDB_COORDINATED=true
export VECTORDB_TLS_CA=/secure/cluster-ca.crt
export VECTORDB_QUERY_REPLICAS=2
export VECTORDB_MAINTENANCE_MS=60000
```

每个节点使用自己的证书和私钥。证书应由该集群 CA 签发，DNS SAN 精确包含节点 ID，EKU 同时允许 serverAuth 和 clientAuth。节点 ID 是不超过 63 字符的小写 DNS label；不要共享节点 ID/证书。连接按注册节点 ID 验证 SAN，RPC 地址可以是可达 IP。不要关闭证书验证。[tonic 服务端双向 TLS 配置](https://docs.rs/tonic/latest/tonic/transport/server/struct.ServerTlsConfig.html)、[客户端身份和服务名验证](https://docs.rs/tonic/latest/tonic/transport/channel/struct.ClientTlsConfig.html)。

以下是 Coordinator 的启动模板，将地址和证书换成实际值：

```sh
export VECTORDB_TLS_CERT=/secure/coord-a.crt
export VECTORDB_TLS_KEY=/secure/coord-a.key
./target/debug/vectordb-cluster --role coordinator \
  --node-id coord-a --zone az-a \
  --bind 0.0.0.0:19531 --advertise http://10.0.0.11:19531 \
  --rpc-bind 0.0.0.0:19541 --rpc-advertise https://10.0.0.11:19541
```

第二个 Coordinator 使用不同 ID、证书、地址与 zone，策略参数必须与首次持久化值相同。writer 节点使用 `--role node`，Query 使用 `--role query-node`，其他身份/RPC 配置方式相同。`--max-shards` 默认为 128，可设 1—512，控制每个 Query 的分片准入；每节点最多 16 个集合、集群最多 32 个成员。每个 shard 的副本尽量分布在不同 zone；不会把同 zone 的两个实例伪装为两个故障域。

向任意 node 创建集合、写入，向 node 或 query-node 查询即可。无需调用 `set_query_routes`，受控模式会拒绝该手动操作。创建时分配尚未完成可能返回 503；继续使用原幂等键重试。未准备好的集合不会回退到完整集合查询来伪装分片已就绪。query-node 主动追赶 WAL，不需要客户端查询来触发；writer 故障接管同样由控制面驱动。

每次启动分配新的 incarnation。成员租约为 10 秒，每 2 秒续租；Coordinator 的独立主租约为 30 秒。失去成员租约的进程永久停止当前身份的服务，恢复网络后应重启。恢复旧 writer 进程不能继续写入；确认结果未知的写操作仍以原幂等键重试。正常退出会撤销成员和拥有的租约。

### 状态、故障与指标

以下接口均需要 Bearer Token：

- `GET /cluster/status`：主 Coordinator、成员、故障域、容量/水位、任务版本、阶段、重试错误、维护序列及 degraded 状态。
- `GET /readyz`：检查成员/控制面及已分配角色的恢复状态；`/healthz` 只表示进程存活。
- `GET /metrics`：Prometheus 文本格式，提供 `vectordb_live_nodes`、`vectordb_pending_assignments`、`vectordb_task_attempts`、`vectordb_last_progress_timestamp_seconds`、`vectordb_degraded`、`vectordb_query_wal_lag_offsets`、`vectordb_snapshot_waiters`、`vectordb_snapshot_wait_ms_total`、`vectordb_snapshot_wait_count`。水位差基于心跳采样，单位为 Kafka offset 差而非精确 mutation 数量。

路由切换前必须完成目标预热。Task Active 和新路由在一个 etcd 事务发布；Coordinator 崩溃后续做原任务。维护任务至少执行一次，在效果已发生但未确认时可能重新计算/发布更新 offset 的检查点；不会覆盖较新状态或重复推进同一迁移的路由 epoch。索引暂时失败可继续 FLAT 查询，原始数据仍能检查点化，任务保留错误并退避重试。

故障后可能临时降级为每分片一个副本，status/metrics 会明确标记；完全没有可用副本则失败。成员租约和 owner fencing 需要真实 etcd/Kafka 可达，不把开发 Compose 的单副本配置当作基础设施 HA。客户端在故障期间应处理可重试错误，不承诺迁移期间所有请求都零失败。

### 排空与受控滚动升级

先部署有容量且协议兼容的新节点，再排空旧节点。例如：

```sh
curl --fail-with-body -X POST http://10.0.0.11:19531/cluster/nodes/query-a/drain \
  -H "Authorization: Bearer $VECTORDB_CLUSTER_TOKEN"
```

排空意图持久化。等待 `/cluster/status` 中所有有关任务 Active、placement 不再引用该节点，再停止进程。容量不足时任务会保留错误或报告降级，不应提前停止旧节点。writer 排空会经过重新分配、fencing 和恢复，可能短暂返回可重试错误。

更新二进制或证书并重新启动；相同 ID 注册新 incarnation 前必须等旧身份退出/租约失效。然后调用 `POST /cluster/nodes/query-a/resume`，等待预热、Active 和 readiness。一次操作一个节点/故障域，最后确认副本数和 degraded 恢复正常。

当前内部协议支持范围为 `[1,1]`，请求/响应版本、成员版本范围和集群身份都必须匹配；不兼容节点或请求明确拒绝，不自动降级为 HTTP。滚动升级只适用于双方均支持协议 1 且状态格式兼容的版本。格式迁移或不相交版本范围必须另做停机迁移方案，不能把当前同版本重启测试当成未来所有版本组合都可滚动。

### M5 本机验收

```sh
cargo build -p vectordb-cluster --bins --examples --locked
python3 -u scripts/cluster_m5_smoke.py
```

脚本自动生成临时 CA/节点证书并启动独立进程，测试证书只供本机验证。覆盖自动调度、WAL 追赶、加入/排空迁移、主备切换、旧 writer 复活拒绝、Query 冷恢复、mTLS/协议拒绝、真实 S3 中断后的维护重试、三个任务崩溃边界，以及同一 zone 标签下整组计算进程同时失效。它会明确标注基础设施仍然在线；多主机网络分区和物理故障域尚未验收。

受控节点分别限制 32 个 HTTP 工作请求和 32 个 RPC 工作请求。迁移可同时保留当前和在途两个缓存版本；资源计费不是硬 RSS 限额。RPC body 最多 1 MiB，响应约 32 MiB，deadline 最多 45 秒，分片查询仍受 M4 的 30 秒总截止时间和合并预算约束。当前维护任务在 writer 执行，独立 Data worker 和生产容量隔离属于后续部署完善范围。

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

命名空间及路由名字限 1..64 个 ASCII 字母、数字、`_`、`-`。集合定义至多 64 KiB，目录至多 1 MiB、32 个命名空间、16 个活跃集合、256 个保留身份；已删除 generation 按 M6 保留窗口回收，M7 同时回收决定 topic 和准备 topic。每 node 最多缓存 16 个集合、32 个运行或排队请求、两个索引 worker。集群路由凭证最多 32,768 个，在 24 小时有效期后加约一分钟保守余量，由后续新请求原子推进单调时钟并清理旧记录、释放预算；已有请求的重试不新增额度。达到目录/凭证额度时明确拒绝新请求，不驱逐仍有效的绑定。

每集合保留原有预算：当前状态 256 MiB、元数据 1 MiB、幂等结果 16 MiB、索引缓存 256 MiB、构建 256 MiB、查询工作区 128 MiB、读视图 2 GiB。HTTP 请求体 512 KiB、WAL 记录 1 MiB、单对象 64 MiB。这些是计费准入，不能视为进程 RSS 的硬上限；多个集合的内存占用会累加。

单机持久化目录使用原生段，旧 SQLite 文件仍可直接打开。旧 `writer/query/checkpoint` 角色保留用于单集合回归测试，需要显式 collection/generation 及预建 topic；node 不会自动收编这类旧集合。默认集合 WAL 格式为 2，多通道集合增加格式 3 的事务决定。分片集合使用状态 manifest 4，单分片仍写 3；读取兼容 manifest 2/3（按单分片处理）。目录格式为 1，新增可选分片与路由字段。已有集合不自动改变分片数；写出 manifest 4 后，旧二进制无法读取该集合检查点。离线迁移见 [M6 手册](m6-operations.md)。

## 测试入口

依赖启动并建好对象桶后执行：

```sh
cargo test --workspace --locked
cargo +1.89 test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
python3 scripts/cluster_smoke.py
python3 -u scripts/cluster_m3_smoke.py
python3 -u scripts/cluster_m4_smoke.py
cargo build -p vectordb-cluster --examples --locked
python3 -u scripts/cluster_m5_smoke.py
cargo test -p vectordb-cluster --test backends --locked -- --ignored
docker compose -p vectordb-cluster-test -f deployments/cluster/compose.yaml down -v
```

四份集群脚本按顺序运行：旧脚本会暂停 Kafka，M5 脚本会暂停 S3 服务来验证维护任务重试；不要并发运行这些测试。M3 脚本覆盖双节点操作和 12 个崩溃场景，包括已提交但未回复、接管阶段、集合生命周期，以及路由绑定后崩溃再切换别名；还验证同一节点进程内从未知提交结果恢复。调试构建的 `VECTORDB_TEST_CRASH_AT` 仅供测试，release 构建不执行该钩子。

契约和验证记录见 [统一实施计划](implementation-plan.md)，性能历史记录见 [M1 状态基准](benchmarks/m1-baseline.json)、[M2 索引基准](benchmarks/m2-baseline.json) 和 [M4 查询扩展基准](benchmarks/m4-baseline.json)。M5 未宣称完成跨主机故障域、生产吞吐、百万级数据、72 小时稳定性、备份迁移或核心 V1 发布验收。
