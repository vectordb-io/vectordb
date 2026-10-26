# 架构与数据可靠性

## 设计定位

当前是面向单机、小规模工作负载的 Rust 实现。使用 Milvus 的核心概念和部分 REST v2 字段风格，不照搬 Coordinator / Node 分布式架构，不要求部署 etcd、消息队列或对象存储。

```text
Application
  ├─ HTTP → axum / authentication / bounded blocking workers
  └─ Rust → Engine::execute(operation, request)
                     │
             schema validation
                     │
        transactional operation dispatcher
          ├─ filter parser and projection
          ├─ FLAT / IVF_FLAT / HNSW
          ├─ RRF / weighted fusion
          └─ SQLite: metadata + entities + WAL
```

## 模块

| 模块 | 职责 |
| --- | --- |
| `schema.rs` | 类型、维度、主键、默认值、命名与字段校验 |
| `filter.rs` | 有界 lexer / parser、三值 NULL 逻辑、JSON 路径、比较与集合匹配 |
| `index.rs` | 距离函数、IVF 聚类、HNSW 图构建与搜索 |
| `engine.rs` | 请求契约、数据生命周期、事务、查询规划、备份 |
| `server/lib.rs` | HTTP、认证、错误映射、队列限制、指标 |
| `server/main.rs` | CLI、配置、监听与信号处理 |

引擎不依赖 HTTP runtime，可以直接嵌入 Rust 应用。接口接受 JSON 请求，返回原始 `data`；HTTP 层添加 `{code,data}` 外壳。

## 存储契约

使用 SQLite `user_version=1` 标记格式，新版本号文件会被拒绝。数据库、集合、分区、别名和实体均持久化；实体主键以 JSON 编码作为复合主键的一部分，区分数字与字符串。

- `collections` 保存 Schema、加载状态、索引配置和下一自动 ID。
- `entities` 保存 JSON 行、主键和分区。外键级联删除清理分区与集合数据。
- 自动主键与数据在同一个事务内提交；失败批次不会消费 ID。
- 对同一主键的 upsert 是完整实体替换。省略字段按默认值或 nullable 补全，缺少必填字段失败。省略分区名会写入 `_default`。
- 删除物理移除记录；`compact` 执行整个 SQLite 文件的 VACUUM，逻辑上不只影响请求集合。
- SQLite 使用 WAL 和 `synchronous=FULL`。成功写入在 SQLite 提交完成之后返回。实际断电保证仍取决于操作系统、文件系统与硬件是否正确实现同步。
- `serde_json/float_roundtrip` 保留 JSON 数值往返时的 f64 精度。Int64 比较保持整数精度，不通过 f64 比较两个整数。
- 引擎打开数据文件时持有旁路 `.lock` 文件的操作系统排他锁，防止多个引擎实例缓存不一致。不要通过别的程序或路径别名修改正在使用的数据文件。

## 并发与一致性

服务通过一个 Mutex 串行执行数据库操作，每次操作在 SQLite 事务中执行。写入后，后续请求立即可见；只接受 `Strong` 一致性设置。

数据库操作放入 `spawn_blocking`，不阻塞 Tokio 的异步执行线程。最多接受 32 个正在处理或排队的数据库请求，超额返回 503；HTTP JSON body 上限 16 MiB。已有操作不会因为客户端断连被自动取消，所以重试写请求必须考虑上次是否已经提交。

串行化有意简化正确性，当前没有高并发读扩展；这些限制不等价于生产容量承诺。

## 索引与搜索

- **FLAT**：扫描全部向量，精确度量并排序。用于正确性基线。
- **IVF_FLAT**：确定性初始化、8 轮 Lloyd 聚类；COSINE 使用归一化数据更新质心。按 `nprobe` 选择倒排桶，再对候选精确打分。`nprobe >= nlist` 时遍历全部桶，结果与 FLAT 一致。
- **HNSW**：确定性伪随机几何层级、逐层贪心导航、底层候选 beam、双向边与邻居裁剪。采用最近邻裁剪变体，尚未实现原论文的多样性邻居启发式。`M`、`efConstruction`、`ef` 控制图与搜索规模。算法参考 [Malkov & Yashunin](https://arxiv.org/abs/1603.09320)。
- 稀疏向量使用精确 IP；二进制向量使用精确 HAMMING / JACCARD。它们当前只支持 FLAT。

索引配置持久化，派生索引驻留内存并延迟构建。写入 / 删除 / 分区删除后失效，下一次搜索重建。加载 / 释放是一种访问控制状态，释放也清理索引缓存，不代表 Milvus 的 segment residency 调度。

当前每次搜索会读取集合行数据，且为分区和过滤额外读取选择数据；JSON 向量存在额外内存开销。ANN 减少候选距离计算，不消除全表读取，因此目前不应宣称具有 Milvus 级规模或性能。没有 SIMD、量化、mmap、增量图更新或异步构建。

过滤先确定允许的实体集合；ANN 在全体向量上选候选，再与允许集合相交。严格过滤下可能少于 `limit`，应增加 `ef` / `nprobe` 或使用 FLAT。范围边界遵循度量方向。相同分数使用稳定实体顺序打破平局。

## 排序与融合

普通查询支持多个标量排序字段，默认按存储主键文本顺序返回，不承诺默认数值 ID 升序。所有分页都属于请求时刻的事务视图；多次请求间写入会改变分页结果。

RRF 按 `1/(k+rank)` 求和，rank 从 1 开始。Weighted 对每个子搜索分数归一化后乘权重求和：COSINE `(1+s)/2`，IP `0.5+atan(s)/π`，距离类 `1-2*atan(d)/π`。只融合各子搜索返回的候选；未入选候选贡献为零。多查询按 `queryIndex` 分组，互不混合。

## 故障处理与后续演进

Schema 错误、主键冲突及不支持参数发生时回滚整个操作。存储错误对客户端返回通用 500，详细原因进入服务日志，避免泄漏本机路径。损坏文件不自动重置。备份采用 SQLite snapshot API，输出不能覆盖已有文件。

后续应按 [功能对照](compatibility.md) 演进：先改善独立引擎的增量索引、列式布局和并发，再引入分布式日志、分片、复制和协议适配。当前没有伪造成功的分布式端点。
