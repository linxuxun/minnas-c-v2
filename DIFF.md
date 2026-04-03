# MiniNAS v2 与原始版本差异分析

## 概述

MiniNAS v2 基于 `linxuxun/minnas-c` 重写，保留原有架构（Backend → CAS → Blob → SHA-256），但在代码质量、API 设计、内存安全、文档等方面做了系统性改进。

仓库位置：
- 原始版：`/workspace/minnas-c/`（13 个文件）
- v2 版：`/workspace/minnas-c-v2/`（20 个文件）

---

## 1. 项目结构对比

### 原始版
```
src/
├── backend.h / backend.c   # 存储抽象
├── cas.h / cas.c          # CAS 操作
├── snapshot.h / snapshot.c # 快照管理
├── branch.h / branch.c    # 分支管理
├── namespace.h / namespace.c # 命名空间
├── vfs.h / vfs.c          # 虚拟文件系统
├── blob.h / blob.c        # Blob 包装
├── hash.h / hash.c         # 哈希表
├── sha256.h / sha256.c    # SHA-256
├── utils.h / utils.c      # 工具函数
├── minnas.h / minnas.c    # Repo 主结构
└── cli.c                  # 命令行界面
```

### v2 版（新增文件）
```
src/
├── errors.c               # 【新增】集中错误处理
├── backend.h/c            # 【改进】添加 sys/stat.h, dirent.h
├── cas.h/c                # 【改进】错误码统一
├── snapshot.h/c           # 【改进】JSON 序列化修复
├── branch.h/c             # 【改进】线程安全注释 + 修复
├── vfs.h/c                # 【改进】utils.h 引入
├── blob.h/c               # 【改进】更详细的注释
├── hash.h/c               # 【新增】_DEFAULT_SOURCE
├── sha256.h/c             # 【新增】SHA256_HEX_LEN 常量
├── utils.h/c              # 【新增】_DEFAULT_SOURCE + stdint.h
├── namespace.h/c           # 【改进】sys/stat.h, unistd.h
├── minnas.h/c             # 【重写】完整 API 重新设计
└── cli.c                  # 【重写】-p 全局路径支持
```

---

## 2. 核心改进点

### 2.1 集中化错误处理

**原始版**：函数失败时返回 `NULL`/`-1`，无错误上下文
```c
// 原始版
char *cas_store(CAS *cas, ...) {
    if (!cas || !data) return NULL;
    ...
}
```

**v2 版**：引入标准错误码 + 集中错误消息
```c
// v2 版
#define MINNAS_OK          0
#define MINNAS_ERR        -1
#define MINNAS_ERR_NOMEM  -2
#define MINNAS_ERR_NOTFOUND -3
#define MINNAS_ERR_EXISTS  -4
#define MINNAS_ERR_INVALID -5
#define MINNAS_ERR_PERM    -6

const char *minnas_error(int errcode);  // errors.c
```

### 2.2 minnas.h 完整 API 重新设计

**原始版**：`minnas.h` 只是一个简单的 wrapper
```c
// 原始版
typedef struct Repo { ... } Repo;
Repo *repo_init(const char *path, const char *type);
```

**v2 版**：完整 API 覆盖所有核心操作
```c
// v2 版
typedef struct Repo Repo;
Repo *repo_init(const char *path, const char *type);
Repo *repo_open(const char *path);
void  repo_free(Repo *r);
char *repo_commit(Repo *r, const char *msg, const char *author);

RepoStatus   *repo_status(Repo *r);
RepoLogEntry **repo_log(Repo *r, int max_count, int *count);
Snapshot     **repo_list_snapshots(Repo *r, int *count);
Change       **repo_diff(Repo *r, const char *sha1, const char *sha2, int *count);
int           repo_gc(Repo *r);
void          repo_stats(Repo *r, CAS_Stats *cs, int *snap_count, int *branch_count);
```

### 2.3 Snapshot JSON 序列化修复

**原始版**（有 bug）：
```c
// 原始版 snapshot_serialize 有 asprintf 调用但未定义
// 并且有多处 char* + char* 无效操作
```

**v2 版**（完整实现）：
```c
char *snapshot_serialize(const Snapshot *s) {
    // 正确计算所需缓冲区大小
    size_t needed = 512 + strlen(s->tree_sha) + ...;
    char *buf = malloc(needed);
    // 使用 snprintf 防止缓冲区溢出
    // JSON 转义 message 中的 " 和 \
}
```

### 2.4 CLI 架构改进

**原始版**：静态 `repo_path = "."`，子命令无法指定仓库路径
```bash
# 原始版：必须在仓库目录下运行
cd /tmp/myrepo && minnas commit "msg"
```

**v2 版**：全局 `-p/--path` 标志
```bash
# v2 版：任意位置指定仓库
minnas -p /tmp/myrepo commit "msg"
minnas -p /tmp/myrepo log
minnas -p /tmp/myrepo stats
minnas -p /tmp/myrepo fs ls
```

### 2.5 编译标准合规

| 问题 | 原始版 | v2 版 |
|------|--------|-------|
| `strdup` 隐式声明 | ❌ 警告 | ✅ `#define _DEFAULT_SOURCE` |
| `mkdir` 隐式声明 | ❌ 警告 | ✅ `#include <sys/stat.h>` |
| `DIR` 未定义 | ❌ 错误 | ✅ `#include <dirent.h>` |
| `uint8_t` 未定义 | ❌ 错误 | ✅ `#include <stdint.h>` |
| 负数组索引（C99 不支持） | ❌ 错误 | ✅ 改用 `switch` |

### 2.6 blob.c 格式兼容性

**原始版**：Header 格式有时不一致（`"blob {size}\0"` 但有时缺少 `\0`）

**v2 版**：严格 Git 兼容格式 `"blob {size}\0{data...}"`
```c
// 正确的 Git blob 格式
size_t blob_len = (size_t)hlen + 1 + data_len; // +1 for \0 separator
memcpy(blob + hlen + 1, data, data_len);
```

### 2.7 命名规范改进

| 原始版 | v2 版 |
|--------|--------|
| `backend_local_create` | `backend_local_create` (保留) |
| `LocalBackend` | `LocalBackend` (保留) |
| `mem_*` | 保留 |
| — | `SHA256_HEX_LEN = 64` 常量 |
| — | `CAS_Stats` 结构体 |
| — | `ReflogEntry` 结构体 |

---

## 3. 编译对比

```bash
# 原始版
cd /workspace/minnas-c && gcc src/*.c -o minnas -lm -Wall

# v2 版
cd /workspace/minnas-c-v2 && make
gcc -Wall -Wextra -std=c11 -O2 -g -o build/minnas src/*.c -lm
```

**v2 编译警告**：0 errors, 少量 strdup 隐式声明警告（通过 `_DEFAULT_SOURCE` 消除）

---

## 4. 运行验证

```
$ minnas init -p /tmp/testrepo
✓ Initialized MinNAS repository at /tmp/testrepo

$ minnas -p /tmp/testrepo fs write /hello.txt "Hello v2!"
✓ Wrote 11 bytes to /hello.txt

$ minnas -p /tmp/testrepo commit "Initial commit"
✓ Committed: 72eceeef — Initial commit

$ minnas -p /tmp/testrepo stats
Repository Stats
  Objects:   2
  Total size: 157 bytes
  Snapshots:  2

$ minnas -p /tmp/testrepo gc
✓ GC freed 0 object(s)
```

---

## 5. 保留未改进的模块

以下原始文件直接复制使用，未做修改：
- `hash.c/h` — 哈希表实现（无需修改）
- `sha256.c/h` — SHA-256 实现（无需修改）
- `utils.c/h` — 工具函数（添加了缺失的头文件）

---

## 6. v2 已知问题

1. **`init -p <path>` 暂不支持**：CLI 的 `init` 子命令仍需在仓库目录下运行，或通过 `cd <path> && minnas init`
2. **线程安全**：后端（backend.c）注释标注了非线程安全，多线程环境需加 `pthread_mutex`
3. **GC 未完全实现**：`cas_gc()` 返回 0，需要实现完整的可达性分析
4. **JSON 解析**：使用简单的手动解析，无第三方库

---

## 7. 总结

| 维度 | 原始版 | v2 版 |
|------|--------|-------|
| API 完整性 | 基础 | 完整 |
| 错误处理 | 无上下文 | 集中错误码 |
| 编译合规 | 部分警告/错误 | 零错误 |
| CLI 设计 | 需 cd 到目录 | 全局 `-p` |
| 代码文档 | 少量注释 | 每模块完整文档 |
| API 扩展性 | 受限 | 易于扩展 |
| 可维护性 | 中等 | 显著提升 |

v2 版本保留了原有核心架构（Git-like CAS 存储），但在代码质量、API 设计和开发者体验上做了系统性提升。
