# GitHub PR 同步修复记录

## 背景

组织仓库的 GitHub PR 同步存在两个问题：

1. 定时同步会拉取仓库全部 PR，数据量大、耗时长。
2. 线上环境 `organization_repo_prs` 表 insert 持续失败。

## 问题一：定时同步只拉最近 20 条

### 现状

`src/blog_module.cc` 的 `SyncAllReposFromGitHub()` 每 30 分钟调度一次 PR 同步，
原实现分页拉取全部 PR（`per_page=50`，最多 10 页，跟随 `Link` 头翻页）。

### 改动

- `src/manager/organization_repo_pr_manager.h`
  - `SyncFromGitHub` 新增参数 `int max_prs = 0`（`0` 表示全量）。
- `src/manager/organization_repo_pr_manager.cc`
  - `per_page` 改为 `max_prs > 0 ? max_prs : 50`。
  - 当 `max_prs > 0` 时只取第一页，不再翻页。
- `src/blog_module.cc`
  - 定时同步调用改为 `SyncFromGitHub(id, url, token, 20)`。

手动全量同步接口 `/api/v1/organization/repos/prs/sync` 保持全量行为不变。

传 `id`（本地 PR 记录 id）时走单条同步：拉取该 PR 详情与 reviews 后回写，
GitHub 404 / 限流等错误直接返回非零 errno；传 `repo_id` 时仍为异步全量同步。

## 问题二：PR insert 失败

### 现象

线上 MySQL 日志中 `organization_repo_prs` 的 insert 一直失败：

```text
merged_at = '1970-01-01 08:00:00'   -- epoch 0，超出 TIMESTAMP 范围
```

### 根因

`closed_at` / `merged_at` 是 `TIMESTAMP NOT NULL` 列，MySQL `TIMESTAMP` 的合法范围
固定为 `1970-01-01 00:00:01` ~ `2038-01-19 03:14:07`（UTC）。

同步 / webhook 代码在 GitHub 无对应时间（PR 未关闭、未合并）时写入 `time_t = 0`，
即 epoch 0（东八区显示为 `1970-01-01 08:00:00`），超出 `TIMESTAMP` 范围，导致 insert 报错。

### 修复（代码层）

在 `src/manager/organization_repo_pr_manager.cc`：

1. 新增哨兵函数 `emptyTimestamp()`，返回全库统一的空时间默认值
   `'1980-01-01 00:00:00'`（与所有 TIMESTAMP 列的 DB 默认值一致）。
2. 同步路径 `syncPage`：`closed_at` / `merged_at` 为 0 时写入哨兵。
3. Webhook 路径 `HandlePRWebhook`：同样处理（`opened` 事件的空时间也会触发同样失败）。

`create_time` 已有 `if (si.created_at)` / `if (created_at)` 守卫，不受影响。

### 修复（配置层，可选）

MySQL 从 5.6 升级到 5.7+/8.0 后，默认 `sql_mode` 变为严格模式（含
`STRICT_TRANS_TABLES`），越界时间值由「警告 + 截断」变为「直接报错」。

可通过关闭严格模式让 insert 放行（**全库生效，谨慎使用**）：

```sql
SET GLOBAL sql_mode = REPLACE(@@GLOBAL.sql_mode, 'STRICT_TRANS_TABLES', '');
SET GLOBAL sql_mode = REPLACE(@@GLOBAL.sql_mode, 'STRICT_ALL_TABLES', '');
```

永久生效需写入 `/etc/my.cnf` 的 `[mysqld]` 段：

```ini
[mysqld]
sql_mode = "ONLY_FULL_GROUP_BY,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"
```

## 修改文件清单

| 文件 | 改动 |
|------|------|
| `src/blog_module.cc` | 定时同步传 `max_prs = 20` |
| `src/manager/organization_repo_pr_manager.h` | `SyncFromGitHub` 新增 `max_prs` 参数 |
| `src/manager/organization_repo_pr_manager.cc` | `per_page` 逻辑 + 单页截断 + `emptyTimestamp()` 哨兵 + 同步/webhook 空时间处理 |

## 部署注意

- 代码层修复（哨兵）与配置层修复（非严格模式）可同时生效，互不冲突，推荐保留代码修复。
- 定时同步生效后日志应显示 `per_page=20` 且只跑 page 1；
  若日志仍显示 `per_page=50` 并翻多页，说明是手动全量同步或旧二进制未部署。
- 无法在本地编译验证（项目为 Linux 构建，本机无 Linux 工具链 / ORM 生成器），
  需在生产环境部署后确认 insert 成功。
