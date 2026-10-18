#!/usr/bin/env bash
#
# deploy.sh — 在部署服务器上执行 冷启动 / 热启动 / 停服
#
# 用法:
#   DEPLOY_PATH=/home/web/apps/server CONFIG_DIR=/home/web/apps/conf \
#     ./deploy.sh --mode cold|hot|stop --artifact /tmp/techhaven-server-v1.2.3-amd64.tar.gz
#
#   cold : 停服 → 清空运行目录 → 解压产物 → 用 CONFIG_DIR 覆盖 conf/ → ./server -d 启动
#   hot  : 同步 CONFIG_DIR → conf/ → 拷贝 module/*.so → 向 master 发 SIGHUP → 存活检测
#   stop : 仅执行 ./server -s stop
#
set -euo pipefail

MODE=""
ARTIFACT=""

# 可按需覆盖：服务器二进制名（实际以上面的 conf/system.yml 为准）
SERVER_BIN="${SERVER_BIN:-server}"
# 存活检测窗口（秒）：daemon 在 worker 崩溃后会间隔 restart_interval 重启，故需留出窗口
RELOAD_GRACE_SECS="${RELOAD_GRACE_SECS:-10}"
START_GRACE_SECS="${START_GRACE_SECS:-30}"

usage() { sed -n '2,13p' "$0"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --mode)     MODE="${2:-}"; shift 2 ;;
        --mode=*)   MODE="${1#*=}"; shift ;;
        --artifact) ARTIFACT="${2:-}"; shift 2 ;;
        --artifact=*) ARTIFACT="${1#*=}"; shift ;;
        -h|--help)  usage; exit 0 ;;
        *) echo "未知参数: $1" >&2; usage >&2; exit 2 ;;
    esac
done

: "${DEPLOY_PATH:?需要设置环境变量 DEPLOY_PATH（运行目录）}"
export LD_LIBRARY_PATH="$DEPLOY_PATH/lib:${LD_LIBRARY_PATH:-}"

log() { echo "[deploy] $*"; }
die() { echo "[deploy][ERROR] $*" >&2; exit 1; }

# 用于 -s 命令的 server 二进制（优先 SERVER_BIN，其次同目录下的 server）
server_bin() {
    if   [ -x "$DEPLOY_PATH/$SERVER_BIN" ]; then printf '%s' "$DEPLOY_PATH/$SERVER_BIN"
    elif [ -x "$DEPLOY_PATH/server" ];      then printf '%s' "$DEPLOY_PATH/server"
    else printf '%s' "$DEPLOY_PATH/$SERVER_BIN"; fi
}

# 本部署 server 二进制的真实路径（兼容 ./server 与绝对路径）
server_exe() {
    local b; b="$(server_bin)"
    readlink -f "$b" 2>/dev/null || printf '%s' "$b"
}

# 本部署正在运行的 server 进程 pid（supervisor + worker），不依赖 conf/system.yml。
# 依据 /proc/<pid>/exe 指向 $DEPLOY_PATH/server 判定，规避 work_path 与配置不一致的问题。
server_pids() {
    [ -d /proc ] || return 0
    local exe p real
    exe="$(server_exe)"
    for p in /proc/[0-9]*; do
        [ -e "$p/exe" ] || continue
        real="$(readlink -f "$p/exe" 2>/dev/null || true)"
        [ "$real" = "$exe" ] || continue
        printf '%s\n' "${p#/proc/}"
    done
    return 0
}

server_running() { [ -n "$(server_pids)" ]; }

# 真正处理 SIGHUP 的是 master：新模型下 master fork worker，
# worker 会 signal(SIGHUP, SIG_IGN)，向其发 HUP 不会触发热重载。
# master = 父进程不在本部署 server 进程集合中的那个进程
# （daemon 化后 PPid=1，前台运行时为 shell，均不在集合内）。
server_master_pid() {
    local pids p ppid
    pids="$(server_pids | tr '\n' ' ')"
    for p in $pids; do
        ppid="$(awk '/^PPid:/{print $2}' "/proc/$p/status" 2>/dev/null || true)"
        case " $pids " in *" $ppid "*) ;; *) printf '%s' "$p"; return 0 ;; esac
    done
    return 1
}

wait_server_running() {
    local secs="$1" i
    for i in $(seq 1 "$secs"); do
        server_running && return 0
        sleep 1
    done
    return 1
}

wait_stopped() {
    local i
    for i in $(seq 1 30); do
        server_running || return 0
        sleep 1
    done
    return 1
}

stop_service() {
    local server; server="$(server_bin)"
    if ! server_running; then
        log "服务未在运行（运行目录: $DEPLOY_PATH）"
        return 0
    fi
    log "停止服务: $server -s stop"
    ( cd "$DEPLOY_PATH" && "$server" -s stop ) || true
    if ! wait_stopped; then
        # -s stop 依赖 pid 文件，配置不一致时可能无效，兜底按进程终止
        log "优雅停止超时，改为向进程发送 SIGTERM"
        local p
        for p in $(server_pids); do kill -TERM "$p" 2>/dev/null || true; done
        if ! wait_stopped; then
            log "SIGTERM 超时，强制终止"
            for p in $(server_pids); do kill -KILL "$p" 2>/dev/null || true; done
            wait_stopped || die "服务未能在超时内停止（进程: $(server_pids | tr '\n' ' ')）"
        fi
    fi
    log "服务已停止"
}

start_service() {
    local server; server="$(server_bin)"
    log "启动服务: $server -d"
    ( cd "$DEPLOY_PATH" && "$server" -d ) || die "启动命令执行失败"
    wait_server_running "$START_GRACE_SECS" \
        || die "服务启动后 ${START_GRACE_SECS}s 内未检测到 server 进程存活（运行目录: $DEPLOY_PATH）"
    log "服务已启动 pids=$(server_pids | tr '\n' ' ')"
}

require_artifact() {
    [ -n "$ARTIFACT" ] || die "--mode ${MODE} 需要 --artifact <tar.gz>"
    [ -f "$ARTIFACT" ] || die "产物不存在: $ARTIFACT"
}

# 将 CONFIG_DIR 同步到 $DEPLOY_PATH/conf
# 优先 rsync（原地更新，保留 conf 目录 inode，进程内 inotify 监听不失效）；
# 无 rsync 时回退为整体替换。
sync_config() {
    [ -n "${CONFIG_DIR:-}" ] || die "需要设置环境变量 CONFIG_DIR（指定配置目录）"
    [ -d "$CONFIG_DIR" ] || die "配置目录不存在: $CONFIG_DIR"
    if [ "$(cd "$CONFIG_DIR" 2>/dev/null && pwd -P)" = "$(cd "$DEPLOY_PATH/conf" 2>/dev/null && pwd -P)" ]; then
        die "CONFIG_DIR 不能是 $DEPLOY_PATH/conf 自身"
    fi
    log "同步配置: $CONFIG_DIR -> $DEPLOY_PATH/conf"
    mkdir -p "$DEPLOY_PATH/conf"
    if command -v rsync >/dev/null 2>&1; then
        rsync -a --delete "$CONFIG_DIR"/ "$DEPLOY_PATH/conf"/
    else
        log "未找到 rsync，回退为整体替换 conf/"
        rm -rf "$DEPLOY_PATH/conf"
        cp -a "$CONFIG_DIR" "$DEPLOY_PATH/conf"
    fi
}

case "$MODE" in
    cold)
        require_artifact
        [ -n "${CONFIG_DIR:-}" ] || die "cold 需要设置环境变量 CONFIG_DIR（指定配置目录）"
        [ -d "$CONFIG_DIR" ] || die "配置目录不存在: $CONFIG_DIR"
        # 安全：避免误清空其他服务的运行目录
        if [ -e "$DEPLOY_PATH" ] && [ -n "$(ls -A "$DEPLOY_PATH" 2>/dev/null || true)" ] \
           && [ ! -e "$DEPLOY_PATH/$SERVER_BIN" ] && [ ! -e "$DEPLOY_PATH/server" ] \
           && [ "${FORCE_WIPE:-0}" != "1" ]; then
            die "拒绝清空 $DEPLOY_PATH：目录非空且不含 $SERVER_BIN/server（疑似其他服务目录）。确认无误请设置 FORCE_WIPE=1 重试"
        fi
        stop_service
        log "清空运行目录: $DEPLOY_PATH"
        rm -rf "$DEPLOY_PATH"
        mkdir -p "$DEPLOY_PATH"
        log "解压产物: $ARTIFACT -> $DEPLOY_PATH"
        tar xzf "$ARTIFACT" -C "$DEPLOY_PATH"
        sync_config
        chmod +x "$DEPLOY_PATH/$SERVER_BIN" 2>/dev/null || true
        start_service
        ;;
    hot)
        require_artifact
        server_running || die "服务未运行，无法热启动（请改用 cold）。运行目录: $DEPLOY_PATH"
        if [ -n "${CONFIG_DIR:-}" ]; then
            sync_config
            # 等待进程内 inotify 监听应用新配置，再触发热重载
            sleep 1
        else
            log "CONFIG_DIR 未设置，跳过配置同步（沿用现有 conf/）"
        fi
        tmp="$(mktemp -d)"
        trap 'rm -rf "$tmp"' EXIT
        log "解压产物到临时目录: $tmp"
        tar xzf "$ARTIFACT" -C "$tmp"
        [ -d "$tmp/module" ] || die "产物内没有 module/ 目录"
        log "热更新模块: $tmp/module/*.so -> $DEPLOY_PATH/module/"
        mkdir -p "$DEPLOY_PATH/module"
        mv "$tmp"/module/*.so "$DEPLOY_PATH/module/"
        mv "$tmp/errors.json" "$DEPLOY_PATH/errors.json"
        if [ -d "$tmp/locale" ]; then
            log "热更新国际化目录: $tmp/locale -> $DEPLOY_PATH/locale"
            rm -rf "$DEPLOY_PATH/locale"
            mv "$tmp/locale" "$DEPLOY_PATH/locale"
        fi

        # 优先向 master 发 SIGHUP（触发 master fork 新 worker，不依赖 pid 文件/work_path）
        master="$(server_master_pid || true)"
        if [ -n "$master" ]; then
            log "热重载: kill -HUP $master"
            kill -HUP "$master" || die "向 master($master) 发送 SIGHUP 失败"
        else
            server="$(server_bin)"
            log "热重载: $server -s reload"
            ( cd "$DEPLOY_PATH" && "$server" -s reload ) || die "执行 server -s reload 失败"
        fi

        wait_server_running "$RELOAD_GRACE_SECS" \
            || die "热重载后 ${RELOAD_GRACE_SECS}s 内未检测到 server 进程存活（运行目录: $DEPLOY_PATH）"
        log "热启动完成 pids=$(server_pids | tr '\n' ' ')"
        ;;
    stop)
        stop_service
        ;;
    *)
        die "未知 --mode: '${MODE}'（应为 cold|hot|stop）"
        ;;
esac
