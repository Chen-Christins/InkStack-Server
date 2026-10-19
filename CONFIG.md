# 配置管理指南

本项目的运行配置位于 `bin/conf/`，通过模板 + 环境变量 + 渲染脚本的方式管理，支持在开发与生产环境之间快速切换。

## 1. 文件一览

| 文件 | 说明 |
|------|------|
| `bin/conf/system.yml.tpl` | 运行时配置模板：服务端口、日志、MySQL、Redis、工作线程 |
| `bin/conf/TechHaven.yml.tpl` | 业务配置模板：AI、超级管理员、分词字典/索引路径 |
| `bin/conf/system.yml` | 由模板渲染出的**实际生效**配置（被框架加载，**不入库**） |
| `bin/conf/TechHaven.yml` | 由模板渲染出的**实际生效**配置（被框架加载，**不入库**） |
| `bin/conf/dev.env` | 开发环境变量（入库） |
| `bin/conf/prod.env.example` | 生产环境变量模板（入库） |
| `bin/conf/prod.env` | 生产环境变量（**不入库**，由运维从 example 复制后填写真实值） |
| `switch-env.sh` | 切换/渲染脚本 |

> `bin/conf/` 下只有 `.yml` 会被框架加载；`.tpl` / `.env` / `.md` 不会被加载。

## 2. 切换环境

```sh
# 切换为开发配置
./switch-env.sh dev

# 切换为生产配置（首次需先准备 prod.env）
cp bin/conf/prod.env.example bin/conf/prod.env   # 填写真实值后
./switch-env.sh prod
```

脚本行为：

- 根据仓库根目录自动推导 `WORK_PATH`，无需按机器手动修改绝对路径。
- 优先使用 `envsubst` 渲染，未安装时自动回退到 `sed`。
- 渲染后校验是否残留未替换的 `${...}`，存在则报错并提示缺哪个变量。
- **只切换配置，不重启服务**。切完后需自行重启或发送 `SIGHUP` 触发热重载。

## 3. 新增一个配置项

先判断这个配置项属于哪一类：

- **各环境值不同**（如路径、数据库、日志级别）→ 走占位符 + env 文件。
- **值恒定不变**（如 `admin.account`）→ 直接写死在模板里，不进 env。

下面以新增「上传目录 `upload.dir`」为例，说明完整流程。

### 3.1 在模板中加占位符

按配置所属类别选模板：运行时配置进 `system.yml.tpl`，业务配置进 `TechHaven.yml.tpl`。

```yaml
# bin/conf/TechHaven.yml.tpl
upload:
  dir: ${UPLOAD_DIR}
```

### 3.2 在两个 env 文件里加值

```ini
# bin/conf/dev.env
UPLOAD_DIR=uploads

# bin/conf/prod.env.example（prod.env 同步）
UPLOAD_DIR=/data/techhaven/uploads
```

### 3.3 重新渲染

```sh
./switch-env.sh dev   # 或 prod
```

渲染后 `bin/conf/TechHaven.yml` 里会出现对应的真实值。

## 4. 在 C++ 代码中读取

用 `chen::Config::Lookup` 声明配置项，key 用 `.` 对应 YAML 层级（`upload.dir` ↔ `upload: dir:`），并提供默认值。

```cpp
#include <chen/config/config.h>

// 声明（一般放在源文件顶部，作为 static 全局变量，前缀 g_）
static chen::ConfigVar<std::string>::ptr g_upload_dir =
    chen::Config::Lookup("upload.dir", std::string("uploads"), "上传目录");

// 使用
std::string dir = g_upload_dir->getValue();
```

### 4.1 支持的配置类型

| C++ 类型 | 说明 |
|----------|------|
| `std::string` | 字符串 |
| `bool` | 布尔（YAML 的 true/false/1/0） |
| `int32_t` / `int64_t` / `double` 等 | 数值（经 `boost::lexical_cast` 转换） |
| `std::vector<T>` | 列表 |
| `std::list<T>` | 列表 |
| `std::set<T>` / `std::unordered_set<T>` | 集合 |
| `std::map<std::string, T>` | 键值表（对应 YAML 嵌套映射） |

> 参考现有用法：`src/blog_module.cc` 中的 `mysql.dbs`、`src/index.cc` 中的 `search.jieba_dict_path`、`src/manager/user_manager.cc` 中的 `admin.account`。

### 4.2 配置变更监听（热重载）

框架通过 `SIGHUP` 重载配置，`ConfigVar` 支持注册回调以响应变更：

```cpp
uint64_t id = g_upload_dir->addListener([](const std::string& old_val, const std::string& new_val) {
    // 处理配置变更
});
```

## 5. Redis 对象缓存

高频按 id 查询的实体（用户、文章、评论、通知、组织等）使用 Redis 作为共享缓存，key 形如 `cache:obj:v1:<entity>:<id>`；
每个 Manager 自己维护 key、TTL 和 JSON 反序列化逻辑。点赞、关注、关联表等低频实体已移除进程内 LRU，直接查 DB。

对象缓存默认开启，可通过 `system.yml` 关闭（关闭后所有实体回源 DB）：

```yaml
cache:
  obj:
    enabled: true
```

> 该配置项有代码默认值 `true`，不写也能正常工作；Redis 异常时缓存操作会静默降级为 DB 查询。

## 6. 注意事项

1. **占位符命名**：统一用大写 snake_case（如 `MYSQL_HOST`、`UPLOAD_DIR`），并在 `dev.env` 与 `prod.env.example` 中保持一致。
2. **密钥不入库**：生产密码、密钥只写在 `bin/conf/prod.env`（已被 `.gitignore` 忽略），不要写进 `prod.env.example` 或提交。
3. **改模板后必须重新渲染**：直接改 `system.yml` / `TechHaven.yml` 会在下次 `switch-env.sh` 时被覆盖，请始终改 `.tpl` 和 `.env`。
4. **未定义变量会报错**：模板中出现 `${...}` 但 env 文件里没有对应变量时，脚本会终止并提示。
5. **常量不走占位符**：不会随环境变化的项直接写死，减少 env 文件噪音。
6. **`WORK_PATH` 是内置变量**：部署目录（`work_path`）可直接在 env 文件里配置（如 `prod.env` 中 `WORK_PATH=/home/web/apps/server`）；若未设置则回退为仓库根目录。
