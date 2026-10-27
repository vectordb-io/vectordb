# Python wheel 平台验证记录

日期：2026-10-27。包版本：`vectordb_io 0.1.0`。本记录说明实际执行的构建、安装与功能验证，不代表已上传 PyPI。

## 发布目标与运行验证

| 发布目标 | 构建环境 | 实际运行验证 | 结果 |
| --- | --- | --- | --- |
| macOS 11+ / arm64（M 系列） | Apple Silicon，Rust 1.99.1，maturin 1.15.0 | CPython 3.9.6、3.14.6 | 两个解释器各通过 14 项集成测试 |
| manylinux2014 / x86_64（glibc 2.17+） | Ubuntu 24.04 x86_64，Rust 1.89.0，maturin 1.15.0，Zig 0.13.0 | CPython 3.12.3，宿主 glibc 2.39 | 全新虚拟环境安装 wheel 后通过 14 项集成测试 |

两平台使用同一份引擎与 Python API 源码，基于提交 `df9c887`，附加打包配置、MIT 许可证、CI 和文档改动。构建入口为 `scripts/build_python_wheel.sh`。Linux 的 Rust 依赖从 Cargo.lock 对应的本机缓存经 `cargo vendor --locked --offline` 导出，在服务器上离线编译；SQLite 随扩展一起编译。

## 已验证的行为

- 使用 `pip install --no-index --only-binary=:all: --find-links ... vectordb_io` 在全新虚拟环境安装发布用 wheel，安装环境不需要 Rust/C 编译工具链。
- Linux 上另执行 `pip install . --no-build-isolation` 验证源码安装，安装后再次通过全部 14 项集成测试。此路径生成的本机 `linux_x86_64` wheel 仅用于源码安装验证；发布目录中保留的是经过审计的 manylinux wheel。
- `from vectordb_io import VectorDB`，并在源码目录之外运行，确认包不依赖旧 `vectordb` 模块。
- 建表、insert、完整实体 upsert、get、标量 query、向量 search、delete；过滤、投影、分页及多查询向量。
- 关闭后重新打开文件，写入与删除结果持久保留；子进程 `os._exit()` 绕过正常关闭后，已确认写入仍可恢复。
- Int64 主键和 Unicode 往返、非法向量拒绝、批次失败整体回滚。
- 文件排他锁、重复 close、上下文异常清理、垃圾回收释放锁、快照恢复。
- 多线程访问与并发关闭、fork 后拒绝使用继承的句柄。
- 安装元数据、类型提示、异常模块名及异常 pickle 往返。
- 两个 wheel 均通过 `twine check --strict`；pip 的平台选择检查分别选中 macOS arm64 与 Linux x86_64 wheel。
- 源码分发包保留完整 Git workspace 与原始锁文件；Linux 上从 `.tar.gz` 安装后通过全部 14 项集成测试。源码包及两个 wheel 都带有 MIT 声明、项目链接和依赖许可证文件，并通过发布工作流的产物检查。

## Linux 二进制兼容性

构建使用 `--target x86_64-unknown-linux-gnu --zig --compatibility manylinux2014 --auditwheel check`，没有跳过兼容性检查。额外运行 auditwheel 6.6.0，确认产物符合 `manylinux_2_17_x86_64`。

auditwheel 检测到的系统库为 `libc.so.6`、`libm.so.6`、`libpthread.so.0`、`libdl.so.2`；未依赖系统 SQLite 或 libpython。运行验证在 Ubuntu 24.04 上完成；glibc 2.17 下限由二进制兼容性审计验证，尚未在每一种 Linux 发行版上逐一运行。

## 产物

产物集中保存在仓库的 `dist/python-release/`（已忽略，不提交 Git），附带 `SHA256SUMS`：

| 文件 | SHA-256 |
| --- | --- |
| `vectordb_io-0.1.0-cp39-abi3-macosx_11_0_arm64.whl` | `bb362b1ccb3901eea92fee57f07ecf12eb66b26e75684aae19c9ae368e0edfc4` |
| `vectordb_io-0.1.0-cp39-abi3-manylinux_2_17_x86_64.manylinux2014_x86_64.whl` | `7440deb4e41950e7f34d2133139f48ab0644cf77a99c7f32f6d2815b2ed153ea` |

`cp39-abi3` 面向标准 CPython 3.9+，不包含 PyPy 或 free-threaded Python。未提供 Linux ARM64、Alpine/musl、Intel Mac 或 Windows wheel。

发布时应上传这两个同版本 wheel 及 `vectordb_io-0.1.0.tar.gz`。项目已按维护者选择采用 MIT；PyPI 账号/Trusted Publisher 及实际上传仍属于后续发布准备。本次未执行上传，账号配置见 [发布说明](python-publishing.md)。
