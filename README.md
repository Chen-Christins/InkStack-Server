# 高性能博客后台系统

## 项目简介
本项目是一个基于 C++17 的高性能博客后台系统，采用模块化设计，支持多用户、多组织、文章管理、标签分类、作业分发等功能。系统架构参考主流 Web 后台设计，注重性能、可扩展性和易维护性。

## 技术栈
- C++17
- CMake 构建系统
- SQLite3 数据库（可扩展至 MySQL/PostgreSQL）
- 自研 ORM（XML 配置，自动生成 C++ 数据访问层）
- chen-sdk 基础库
- 多线程/协程支持
- Docker 部署支持

## 主要功能
- 用户注册、登录、权限管理
- 组织（社群/团队）管理及成员关系
- 文章发布、编辑、分类、标签
- 作业（Assignment）分发与提交
- API 文档自动生成（见 API_DOCS.md）
- 日志、配置、定时任务、邮件通知等基础服务

## 目录结构
```
├── bin/                # 前端静态资源与可执行文件
├── build/              # 构建输出
├── chen/               # 基础库与通用模块
├── conf/               # 系统配置文件
├── generator/          # ORM 代码生成器
├── logs/               # 日志文件
├── module/             # 业务模块动态库
├── orm_config/         # ORM XML 配置（表结构）
├── orm_out/            # ORM 生成代码
├── src/                # 业务代码（Manager/Servlets等）
├── tests/db/           # 测试用 SQL 数据
├── CMakeLists.txt      # 构建脚本
├── Dockerfile          # Docker 部署
└── README.md           # 项目说明
```

## 快速开始
1. 安装依赖（C++17 编译器、CMake、SQLite3）
2. 编译项目：
   ```sh
   mkdir build && cd build
   cmake ..
   make -j
   ```
3. 运行服务：
   ```sh
   ./bin/main -s/-d
   ```
4. 可选：使用 Docker 部署
   ```sh
   docker build -t blog-backend .
   docker run -p 8080:8080 blog-backend
   ```

## 配置切换（开发/生产）

配置位于 `bin/conf/`，通过 `switch-env.sh` 在开发与生产环境之间快速切换（日志路径、MySQL、Redis 等）：

```sh
# 切换为开发配置
./switch-env.sh dev

# 切换为生产配置（首次需先复制并填写 prod.env）
cp bin/conf/prod.env.example bin/conf/prod.env
./switch-env.sh prod
```

- 模板文件：`bin/conf/system.yml.tpl`、`bin/conf/TechHaven.yml.tpl`
- 环境变量：`bin/conf/dev.env`、`bin/conf/prod.env`（生产密钥不入库）
- 脚本会根据仓库根目录自动推导 `work_path`，无需按机器修改绝对路径

详细的新增配置项方法见 [CONFIG.md](CONFIG.md)。

## 部署打包

`collect.sh` 会收集部署所需文件并打包为 tar.gz（结构与生产目录一致：`server` / `module` / `lib` / `conf` / `dict` / `errors.json`）：

```sh
# 打包生产配置（配置值含部署路径 WORK_PATH 全部取自 bin/conf/prod.env）
./collect.sh prod

# 打包开发配置
./collect.sh dev
```

> 部署路径 `WORK_PATH` 在 `prod.env` 中配置（`WORK_PATH=/home/web/apps/server`），无需手动传参。

产物：
- `dist/` — 部署目录（可检查）
- `techhaven-server-<时间戳>.tar.gz` — 上传到服务器解压即可

上传后手动启动（示例）：

```sh
# 在部署目录下
LD_LIBRARY_PATH=./lib ./server -d
```

## SDK 依赖管理

`chen-sdk` 来自 [Chen-Christins/chen](https://github.com/Chen-Christins/chen) 的 Release，通过 `dependencies.json` 声明版本与架构，`deps.py` 负责下载/切换：

```json
{
  "chen-sdk": {
    "repo": "Chen-Christins/chen",
    "version": "1.5.3",
    "arch": "arm64"
  }
}
```

| 命令 | 说明 |
|------|------|
| `./deps.py sync` | 按配置下载并解压为仓库根目录 `chen-sdk-<version>/`，成功后删除旧版本 |
| `./deps.py use 1.5.4 --arch amd64` | 修改配置并切换版本/架构 |
| `./deps.py use --archive /tmp/chen-sdk.zip` | 使用本地临时 SDK 包（不改主配置） |
| `./deps.py current` | 查看当前已安装的 SDK |
| `./deps.py list` | 列出远端可用版本与架构 |
| `./deps.py clean` | 删除所有已安装 SDK 与缓存 |

说明：

- 下载资产命名 `chen-sdk-<version>-<arch>.zip`，缓存在 `.deps-cache/`，并用 GitHub Release 的 `digest` 做 sha256 校验（离线时仅告警）。
- 新版本下载、解压、校验成功后才清理旧的 `chen-sdk-*/`、旧 zip 与缓存；加 `--keep-old` 可保留。
- macOS 开发 / Linux 编译：把 `arch` 改成 `amd64`（或 `./deps.py use <version> --arch amd64`）后再同步整目录到 Linux 即可，无需软链。
- CMake 的 `chen-sdk-*` glob 与 `collect.sh` 均无需改动；`deps.py` 不使用软链。

## 相关文档
- [API_DOCS.md](API_DOCS.md) 详细接口说明
- `orm_config/` 下 XML 文件定义所有表结构
- `tests/db/` 下 SQL 文件可用于测试数据导入

## 贡献与交流
欢迎提交 Issue 或 PR，或通过邮件与作者联系。

---
高性能 · 模块化 · 易扩展
