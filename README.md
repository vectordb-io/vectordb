# VectorDB · Rust

使用 Rust 重新实现的单机向量数据库。提供可嵌入引擎、HTTP 服务、事务持久化和独立静态官网。

**状态：0.1.0 Alpha。** 参考 Milvus 的数据模型与 REST v2 请求风格，已实现一组可用的单机功能；尚未实现 Milvus 的大部分分布式基础设施，也不是 Milvus / PyMilvus 的直接替代品。具体边界见 [功能对照](docs/compatibility.md)。旧 C++ 实现、旧文档与旧官网已移除。

## 快速启动

要求 Rust 1.89+、Cargo、C 编译工具链（用于编译内嵌 SQLite）。macOS 使用 Xcode Command Line Tools；Linux 使用系统 C 编译器。无外部数据库服务依赖。

```sh
cargo build --release --locked
./target/release/vectordb serve
```

默认监听 `127.0.0.1:19530`，数据保存在 `data/vectordb.sqlite`。另开终端：

```sh
curl http://127.0.0.1:19530/healthz
bash examples/quickstart.sh
```

示例创建 `demo` 集合、写入两条实体、搜索相似向量并执行标量过滤。再次执行前需删除 `demo` 或换一个集合名。

## 当前能力

- 多数据库、集合、显式 Schema / 快速建表、分区、集合别名、重命名、加载与释放。
- Int64 / VarChar 主键，自动 ID；标量字段、JSON、Array、可空字段、默认值、动态字段。
- 批量 insert / 完整实体 upsert、query / get / delete、计数、排序、分页。
- FloatVector、SparseFloatVector、BinaryVector；COSINE、IP、L2（平方距离）、HAMMING、JACCARD。
- FLAT 精确检索、IVF_FLAT、HNSW；标量预筛选、范围检索、分组检索、多查询向量。
- 多向量检索融合：RRF 与归一化加权排名。
- SQLite WAL 与 FULL 同步级别；批量事务、崩溃恢复、排他进程锁、快照备份、JSONL 流式导入导出。
- Bearer Token、请求大小与并发队列限制、健康检查、Prometheus 请求 / 错误计数器、优雅关闭。

没有自动生成 embedding；向量由应用生成并提交。自有数据库逻辑、过滤器、索引算法和 HTTP 服务使用 Rust；SQLite 是 C 实现的内嵌持久化依赖。

## 请求示例

```sh
curl -sS localhost:19530/v2/vectordb/collections/create \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","dimension":3}'

curl -sS localhost:19530/v2/vectordb/entities/insert \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","data":[
    {"id":1,"vector":[1,0,0],"title":"Rust"},
    {"id":2,"vector":[0,1,0],"title":"Storage"}
  ]}'

curl -sS localhost:19530/v2/vectordb/entities/search \
  -H 'Content-Type: application/json' \
  -d '{"collectionName":"demo","data":[[1,0,0]],"limit":1,"outputFields":["title"]}'
```

返回：`{"code":0,"data":[{"id":1,"title":"Rust","distance":1.0}]}`。

## 嵌入式 Rust

```rust
use serde_json::json;
use vectordb_engine::Engine;

fn main() -> vectordb_engine::Result<()> {
    let mut db = Engine::memory()?;
    db.execute("collections/create", json!({"collectionName":"demo","dimension":3}))?;
    db.execute("entities/insert", json!({"collectionName":"demo","data":[
        {"id":1,"vector":[1,0,0]}
    ]}))?;
    let hits = db.execute("entities/search", json!({
        "collectionName":"demo","data":[[1,0,0]],"limit":1
    }))?;
    println!("{hits}");
    Ok(())
}
```

运行仓库中的完整版本：`cargo run -p vectordb-engine --example embedded`。

## 验证

```sh
cargo fmt --all --check
cargo test --workspace --locked
cargo clippy --workspace --all-targets --locked -- -D warnings
cargo build --workspace --locked
python3 scripts/smoke.py
python3 scripts/check_website.py
```

测试覆盖事务回滚、Schema 校验、过滤、主键精度、加载状态、数据库 / 分区隔离、索引刷新、ANN 对照、混合检索、快照、排他锁、损坏数据库、HTTP 鉴权与并发写入。`smoke.py` 使用临时数据库启动真实进程，验证 SIGKILL 后已确认写入可恢复。它不会使用 `data/` 中的实际数据。

## 文档

- [API 与请求契约](docs/api.md)
- [架构、数据可靠性和性能边界](docs/architecture.md)
- [Milvus 功能对照与后续路线](docs/compatibility.md)
- [部署、鉴权、备份恢复](docs/operations.md)
- [验证记录](docs/validation.md)
- [官网与域名发布](website/README.md)

目录：`crates/engine` 数据库引擎；`crates/server` HTTP / CLI；`docs` 文档；`examples` 示例；`website` 官网。官网独立部署到 GitHub Pages，域名为 [vectordb.io](https://vectordb.io/)。
