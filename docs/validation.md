# 验证记录

日期：2026-10-26。此记录说明已执行的验证及边界，不是生产性能认证。

## 本地环境

macOS / Apple Silicon，Rust 1.99.1，Cargo 1.99.1。依赖已锁定在 Cargo.lock，依赖声明的最大最低 Rust 版本为 1.89.0。另已在官方 Rust 1.89 / Debian Bookworm 容器中，使用本机缓存依赖、禁用网络，完成 Linux ARM64 Release 构建。

## 已验证

当前 Rust 集成测试共 25 项（23 引擎 + 2 HTTP），全部通过。

- Rust workspace 编译、格式检查和严格 Clippy。
- 引擎集成测试：CRUD、Schema、动态字段、默认值、可空字段、主键精度、Int64 自动 ID 回滚及重启、分区与数据库隔离、别名与级联删除。
- 过滤：比较、布尔优先级、JSON 路径、IN / NOT IN、NULL 三值逻辑、LIKE、数组包含、无效表达式拒绝。
- 检索：三种稠密度量、稀疏 IP、二进制 HAMMING、范围 / 分组、多查询向量、RRF / weighted。
- IVF_FLAT：128 条二维向量，全部探测时与 FLAT 的前 12 条完全一致；upsert / delete 后刷新。
- HNSW：固定随机种子，400 条八维向量、12 个查询、每查询 10 条；M=12、efConstruction=100、ef=100，对照 FLAT 的 recall@10 至少 95%；写入新最近点后缓存刷新。该阈值仅适用于测试集，不是生产召回率承诺。
- 持久化：排他引擎锁、损坏文件拒绝、快照恢复、SQLite 故障注入触发整批回滚、JSONL 往返与批次错误语义。
- HTTP：Token、错误状态、无效 JSON、16 个并发 autoID 写请求、真实请求大小上限。
- 真实进程测试：临时数据目录启动服务器，运行 README 示例；SIGKILL 后恢复已确认的数据与自动 ID；HNSW 配置重启保持；flush / compact；CLI 快照恢复。
- 官网：本地 HTTP 200，桌面与 390×844 手机浏览器截图检查，无横向溢出；度量 / 查询点 / Top K 切换、代码切换与复制；浏览器无 error / warn 日志。
- 发布：代码提交 `e0baaf9` 已推送 main；[GitHub Rust CI](https://github.com/vectordb-io/vectordb/actions/runs/40340455059)（Ubuntu / macOS）和 [GitHub Pages 部署](https://github.com/vectordb-io/vectordb/actions/runs/40340454733) 均成功。
- 域名：新官网 `https://vectordb.io/` 返回 200；首页、CSS、JavaScript、favicon 和 CNAME 的 SHA-256 与本地文件一致。原 GitHub Pages 地址仍跳转到该域名，线上浏览器也已核对新页面。

## 未验证 / 未完成（提交本记录时）

- 分布式功能、Milvus gRPC / SDK 兼容性、RBAC、BM25、GPU 索引。
- 大数据量压测、长时间稳定性、真实断电、磁盘满及网络分区测试。

## 容器验证的环境限制

标准多阶段 Docker 构建在容器内更新 crates.io 索引时停滞，已取消。改用缓存依赖离线编译后，Rust 1.89 的 Linux Release 二进制构建成功，并根据 Dockerfile 相同的运行阶段生成了本地镜像。

附加 Rust 1.89 测试编译期间宿主磁盘空间不足，随后 Docker 元数据出现 I/O 错误。已清理本次生成的宿主构建缓存并保留本机可执行文件；没有清理其他项目数据。由于恢复 Docker 会影响其他在运行的工作负载，未擅自重启。

因此，**最低版本的 Release 编译已验证，但该版本的完整测试和容器运行 / 重启测试尚未验证通过**。恢复环境后可运行 `VECTORDB_DOCKER_IMAGE=vectordb:rust-rewrite python3 scripts/container_smoke.py` 补充检查。测试容器 `vectordb-tests-e0baaf9` 的清理也被 Docker 元数据错误阻止，需要在恢复 Docker 后移除。
