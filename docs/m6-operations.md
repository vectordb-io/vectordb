# M6 运行、迁移与恢复

## 选择数据入口

`vectordb serve` 在新目录使用 `data/vectordb.vdb/`。如果原默认位置已有 `data/vectordb.sqlite`，服务继续打开该文件。Rust 的 `Database::open` 和 Python 的 `VectorDB(path)` 使用同一选择规则：已有普通文件或以 `.sqlite`、`.sqlite3`、`.db` 命名的新文件使用 SQLite，新建的其他路径使用原生段目录。Python 的 `VectorDB()` 和 `:memory:` 保留内存 SQLite 语义。单机原生模式支持核心集合、别名、分区、索引和实体操作。原有 SQLite 的 load/release、过滤删除等扩展仍可通过 SQLite 入口使用。原生模式按主键删除，用 `delete_ids(collection, ids)`。

原生单机进程独占目录，用 fsync 和原子 rename 提交带 SHA-256 的本地 WAL，包括 catalog 变更；每 128 次变更或显式 flush 时发布 HEAD，随后清理已覆盖的 WAL 和快照。单机和分布式加载相同的 Parquet/向量段和持久化索引，执行同一个 SegmentEngine。备份原生目录使用 `vectordb backup --data /data/app.vdb --output /backup/app.vdb`，输出可直接用单机入口打开。复制运行中的目录可能跨越两个 HEAD。

## 集群维护

受控节点按 Coordinator 的维护周期生成全状态检查点，重写当前段与索引、回收内存墓碑、截断 Kafka、清理旧 manifest；索引构建失败时可使用原始数据继续检索。当前实现按集合压缩，每次压缩会读写该集合的活跃状态。手动触发可用管理凭据调用 `collections/compact`。管理员可用 `collections/storage_stats` 获取集合的 S3 对象数与字节数、Kafka low/high offset、检查点位置和内存状态估算。S3 统计会分页列举对象，属于运维接口。

- 受控模式默认保留检查点前 64 个 Kafka offset；事务标记也占 offset。可用 `VECTORDB_WAL_GRACE_OFFSETS` 调整。落后于 low offset 的消费者从最新检查点重建；writer 接管时把已回放的尾部计入自动检查点阈值。
- GC 仅清理低于已发布 offset、超过五分钟宽限且不属于受保护 retired manifest 的孤儿对象。读取和备份在持有 pin 时验证检查点及对象校验和。每次备份另外保存独立数据副本。
- 已删除的 generation 保留完整的 24 小时幂等窗口及两分钟余量，然后从目录移出并关闭新 owner、pin 与 manifest 发布。额外五分钟宽限和已有 pin 释放后清除 S3 状态和 Kafka topic。老版本缺少删除时间的记录从首次观察开始计时。etcd 保留一个很小的永久关闭标记，防止暂停的旧进程重新发布。
- `VECTORDB_COLLECTION_BYTES` 默认 256 MiB，限制单集合当前行与元数据的估算字节数，超限的新 insert/upsert 返回 409。每个集合还有 16 MiB 幂等结果预算，耗尽返回 503。依赖不可用时保留恢复链，恢复存储后用原请求键重试。Kafka 实际磁盘回收还受到 broker 段滚动、删除延迟及副本数影响；S3 开启版本保留时还需配置非当前对象版本生命周期。
- `VECTORDB_NAMESPACE_COLLECTIONS` 默认 16；由 etcd 保存集群配置，创建集合与目录 CAS 一起判定配额。所有网关使用相同值。每个网关按读、写、管理三类凭据独立执行每秒请求上限，默认各 1,000，可用 `VECTORDB_REQUESTS_PER_SECOND` 调整。限流返回 429。

## 客户端和权限

`VECTORDB_CLUSTER_TOKEN` 是管理凭据。`VECTORDB_WRITE_TOKEN` 可以查询和写实体，`VECTORDB_READ_TOKEN` 仅允许只读数据操作。可选 token 和管理 token 都至少 16 字节且互异。集群管理、维护、状态、指标和旧式内部 HTTP 端点要求管理凭据；受控服务间使用 mTLS。客户端入口的 HTTPS 可在网关前终止。

Python 客户端随包提供，Rust 的同步客户端位于 `vectordb_cluster::client::Client`。两者均返回包含 read/commit token 的完整响应。每个逻辑写操作生成一次幂等键，超时、503 后重试使用同一键和请求体。

```python
from vectordb_io import ClusterClient
import os

db = ClusterClient(os.environ["GATEWAY"], os.environ["VECTORDB_WRITE_TOKEN"])
key = db.new_key()
result = db.execute("entities/upsert", {
    "collectionName": "items", "data": [{"id": 7, "vector": [1, 0]}]
}, key)
```

## 一致备份和独立恢复

命令使用与相应集群相同的 `VECTORDB_ETCD`、`VECTORDB_KAFKA`、`VECTORDB_S3_*` 和管理 token。备份程序需要写入管理端点、读取 S3/Kafka 以及获取 etcd 维护租约的权限。备份目录由操作系统权限保护，选择独立介质，并对目录设置备份保留策略。敏感实体、动态字段和请求结果都会进入备份。

```sh
# 环境变量指向源基础设施，gateway 是源集群的 node HTTP(S) 地址
vectordb-cluster --role backup --cluster source --gateway "$GATEWAY" --archive /backup/run-001

# 环境变量切换到目标基础设施，使用新的集群 ID 和空逻辑目录
vectordb-cluster --role restore --cluster restored --archive /backup/run-001
```

备份租约冻结新 DDL 和实体写入。正在进行的集合创建或删除会使本次备份返回冲突，待生命周期稳定后可重试。工具在每个活跃集合获取有序屏障、flush、pin 并验证段，从已删除 generation 的终态和已提交尾部收集窗口内结果。写完集合文件和 catalog 后校验租约、同步文件并写 `COMPLETE`。崩溃后租约在约 30 秒内过期；未完成的目录不用于恢复，重试使用新的输出目录。

恢复要求新 cluster ID、空 catalog 和完整归档。工具检查 SHA-256，重建主键、自动 ID、分区、索引、别名及幂等结果，在新的 Kafka 日志起点发布检查点，再从 S3 读回并核对实体内容摘要。目录名和别名最后在一个 etcd 事务中可见。目标保留物理 collection ID 并分配新 generation；旧 commit token 属于源集群。源路由和租约不参与恢复。中断后对同一目标、同一归档重新运行；不可见的旧暂存 generation 进入后台回收。验收时另外核对业务查询、过滤、搜索、重复写返回和自动 ID，再切换客户端地址。

## 从 SQLite 或历史 P1 离线迁移

停写 SQLite，用 `vectordb backup` 生成独立一致快照。迁移器使用只读模式，默认保留 Float64 精度。显式传入 `--dtype Float32` 才会量化，并报告最大绝对误差；不支持的名称、未加载的集合或分区、超出目标上限的目录会被拒绝。

```sh
vectordb backup --data /old/data.sqlite --output /backup/old.sqlite
python3 scripts/migrate_sqlite.py /backup/old.sqlite /backup/sqlite-archive
vectordb-cluster --role restore --cluster migrated --archive /backup/sqlite-archive
```

对于 P1 格式 1，先停止客户端写入，用 P1 checkpoint 角色生成最终检查点并保存发布的 manifest 和对应 S3 对象。迁移工具验证物理摘要、scope、递增 offset、epoch、逐条请求摘要和原始执行结果，然后通过相同的 SQLite 转换流程导入。源 WAL 在所选检查点之后仍有写入时，先重新生成最终检查点。

```sh
vectordb-p1-export --manifest /backup/p1-manifest.json --checkpoint /backup/p1-state.json --output /backup/p1.sqlite
python3 scripts/migrate_sqlite.py /backup/p1.sqlite /backup/p1-archive
```

P1 的旧键和目标 `v2-<issuedUnixMs>-<nonce>` 分属两个协议；历史中仍可能被目标协议接受的键会作为已退休凭证迁移，原集合重试返回 409，迁移报告列出保留数量。切换前保留旧数据并核对结果。目标已接收新写入后，回切需要同步增量或重新停写迁移。

## Helm 部署与故障处理

构建计算镜像并在 values 中指定可由 Kubernetes 拉取的镜像版本。chart 部署计算服务。etcd、Kafka 和 S3 使用已有端点，Kafka 首选三副本且 broker 和事务内部 topic 配置 `min.insync.replicas=2`。Kafka 当前适配器使用明文 broker 通道，etcd HTTP gateway 也依赖受信基础设施网络；如部署要求基础设施原生认证/TLS，需要额外接入。集群服务的 mTLS 证书不提供这些后端凭据。

```sh
docker build -f deployments/cluster/Dockerfile -t "$IMAGE" .
helm lint deployments/helm/vectordb
helm upgrade --install vdb deployments/helm/vectordb -f my-values.yaml --wait
```

- 在 `members` 中逐一填写唯一 ID、role、zone 和自身的 TLS Secret。zone 要与真实节点的 `topology.kubernetes.io/zone` 标签相符。每个 Secret 含 `ca.crt`、`tls.crt`、`tls.key`，证书 DNS SAN 为该节点 ID，含 clientAuth 和 serverAuth。管理、读、写 token 和 S3 密钥放入 `credentialsSecret` 对应的同名环境变量键。
- chart 为每个身份建立独立 StatefulSet 和可供预热使用的 headless Service，对客户端暴露仅选择就绪 writer 网关的 ClusterIP。按物理故障域设置资源和副本数；chart 包含角色级 PDB、反亲和偏好和就绪探针。接入方配置外部 HTTPS。
- 同一受支持版本滚动维护时先调用 `POST /cluster/nodes/{id}/drain`，用 `GET /cluster/status` 验证分片副本已迁出且其余节点容量足够，再逐个替换 Pod。StatefulSet 使用 OnDelete；Helm 更新模板后显式替换已排空成员，恢复就绪后调用 `resume`。Coordinator 一次替换一个候选。测试覆盖同版本安装和 upgrade；异版本组合单独认证。已有 M5 部署启用 M6 写冻结和回收前，所有可写入该集群的计算进程必须使用 M6 代码。
- 503 时检查维护冻结、etcd 租约、Kafka ISR 和 S3 容量/权限；429 时降速并退避；数据校验失败时隔离损坏对象，从独立备份恢复。禁止手工删除未覆盖的 Kafka 前缀。扩充节点后先观察 Pending/Warming 再恢复流量。

## 复现验收

`scripts/cluster_m6_smoke.py` 使用专用开发 Compose 项目作为源，自动创建和清理另一套空的 etcd/Kafka/S3 做恢复。另有后端契约测试验证 pin 和过期 generation 的 GC。部署测试 `scripts/cluster_m6_helm_smoke.py --kind <专用本机集群>` 使用已载入测试镜像的单节点 kind 集群，并连接开发 Compose 后端。

```sh
python3 scripts/cluster_m6_soak.py --local --seconds 600 --interval 3 --maintenance-every 4 --backup-every 16 --report local-soak.json
python3 scripts/cluster_m6_soak.py --gateway "$GATEWAY" --seconds 259200 --report release-soak.json
VECTORDB_M4_BINARY=target/release/vectordb-cluster python3 scripts/cluster_m4_smoke.py --release-gates --output scaling.json
```

持续测试会在专用集群创建集合，验证每轮 upsert/delete 后的完整内容，并采样实际对象大小和 Kafka 水位。72 小时任务须在约定发布环境完成。单机、故障域和吞吐的实测记录见[统一计划](implementation-plan.md)。
