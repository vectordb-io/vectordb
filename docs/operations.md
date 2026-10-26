# 运行、备份与部署

## 服务配置

```sh
cargo build --release --locked
./target/release/vectordb serve --bind 127.0.0.1:19530 --data ./data/vectordb.sqlite
```

环境变量：`VECTORDB_BIND`、`VECTORDB_DATA`、`VECTORDB_API_KEY`、`RUST_LOG`（默认 info）。显式 CLI 参数优先于相应环境变量。API key 建议由环境或部署 secret 注入，避免放入命令行历史。空 key 会被拒绝。

默认仅接受本机连接；如需远程访问，用 TLS 反向代理接入，并配置 API key。服务本身没有 TLS、RBAC、用户管理或公网限流。API key 是服务级共享凭据，不能提供租户权限隔离；dbName 仅实现数据命名隔离。

SIGINT / SIGTERM 停止接收新连接并等待请求完成。退出不会删除数据；SQLite 在下次打开时恢复 WAL。不要在运行时手工删除 `.sqlite-wal`、`.sqlite-shm` 或移动 SQLite 文件。

## 备份与恢复

先优雅停止服务，再执行：

```sh
./target/release/vectordb backup \
  --data data/vectordb.sqlite \
  --output snapshot-001.sqlite
```

备份命令不能与持有同一数据文件锁的服务器并行运行，也不会覆盖已有输出。Rust 嵌入应用可在同一 Engine 实例上调用 `backup` 创建一致快照。索引配置、Schema、自动 ID、加载状态和实体都在快照中，派生 ANN 索引在需要时重建。

恢复：停止服务器，保留原数据目录作为回滚来源，把快照复制到一个新的空目录，然后用新的 `--data` 路径启动。不要用快照覆盖一个仍带有旧 WAL / SHM 的数据库路径。

```sh
mkdir restored-data
cp snapshot-001.sqlite restored-data/vectordb.sqlite
./target/release/vectordb serve --data restored-data/vectordb.sqlite
```

`flush` 执行 WAL checkpoint；它不是备份。`compact` 执行同步整库 VACUUM，期间串行队列中的请求需要等待。

## Docker

```sh
docker build -t vectordb:local .
docker run --rm -p 127.0.0.1:19530:19530 \
  -v vectordb-data:/data vectordb:local
```

镜像构建自本仓库，未假设任何已发布 Docker Hub 镜像。容器内监听 `0.0.0.0:19530`，上述命令仅将宿主机回环端口映射出去。默认使用 uid 10001；使用宿主目录 bind mount 时需提前为该 uid 配置读写权限。

## 验证与容量边界

先用 FLAT 对照业务结果，再评估 ANN recall 与 latency。当前实现采用 JSON 行存储、串行请求执行、全表读取和按需整体索引重建，适合开发与小数据集评估；尚没有生产吞吐、内存上限、百万级规模或高可用保证。

HTTP body 16 MiB，批量写入最多 10000 行，batch search 最多 128 个查询，队列最多 32 请求。向量必须维度一致且数值有限。队列满返回 503；客户端应使用有上限的指数退避。写请求连接失败不表示未提交，需要基于业务主键核对状态。

旧 C++ 数据格式没有自动迁移器；如需迁移旧数据，应通过应用导出实体后调用新 API 导入。

## JSONL 导入导出

服务停止后，可在命令行流式导入 / 导出实体：

```sh
./target/release/vectordb export --collection demo --output demo.jsonl
./target/release/vectordb import --collection demo --input demo.jsonl --upsert --batch-size 1000
```

`--database` 缺省 default，`--data` 缺省 data/vectordb.sqlite。导入目标集合必须已经存在且 Schema 匹配，写入 `_default` 分区。每行一个 JSON 对象，空行跳过，单行最多 16 MiB。自动 ID 集合导入带 ID 的导出数据需要 `--upsert`。

**导入以 batch-size 为事务边界**：后续批次失败时，之前成功的批次仍然存在；不是整文件原子导入。批量上限 10000。导出只包含实体，不包含 Schema、索引、别名和分区归属；完整恢复请使用快照备份。输出文件必须不存在；写入失败会清理本次创建的不完整输出。
