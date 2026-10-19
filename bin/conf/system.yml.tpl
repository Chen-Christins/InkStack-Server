# Chen Blog System Configuration
# This configuration file defines the settings for the Chen Blog system, including server configurations, worker threads, logging, database connections, and Redis settings.

# Servers Configuration, defining the HTTP and WebSocket servers with their respective settings.
servers:
  - address: ["0.0.0.0:8090"]
    keepalive: 1
    timeout: 1000
    name: chen-http/1.0.0
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: http
    negotiateH2: 1
    args:
      accept_qps: 1000      # 每秒最多接受的新连接数
      accept_burst: 2000    # 瞬时突发上限（缺省 = accept_qps）
      max_conn: 10000       # 最大并发连接数

  - address: ["0.0.0.0:8091"]
    keepalive: 1
    timeout: 120000
    name: chen-ws/1.0.0
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: ws

  - address: ["0.0.0.0:8092"]
    keepalive: 1
    timeout: 1000
    name: chen-rpc/1.0.0
    accept_worker: accept
    io_worker: io
    process_worker: io
    type: rpc

fiber:
  stack_size: 65536

# Worker threads configuration, defining the number of threads for IO and accept workers.
workers:
  io:
    thread_num: 1
  accept:
    thread_num: 1

# Logging configuration, defining different loggers for root, system, and access logs with their respective levels, formatters, and appenders.
logs:
  - name: root
    level: ${LOG_LEVEL}
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/root.log
      - type: StdoutLogAppender
  - name: system
    level: info
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/system.log
      - type: StdoutLogAppender
  - name: access
    level: info
    appenders:
      - type: FileLogAppender
        file: ${WORK_PATH}/logs/access.log
      - type: StdoutLogAppender

# Database configuration for MySQL, defining the database path and SQL settings for the blog database.
mysql:
  dbs:
    techhaven:
      dbname: ${MYSQL_DBNAME}
      host: ${MYSQL_HOST}
      port: ${MYSQL_PORT}
      user: ${MYSQL_USER}
      passwd: ${MYSQL_PASSWD}

# Redis configuration for Fox Thread, defining the Redis settings for the Fox Thread component, including the name, number of connections, and advance settings.
fox_thread:
  redis:
    name: redis
    num: 1
    advance: 0

# Redis configuration for the blog, defining the host, type, pool size, and timeout settings for the Redis connection used by the blog.
redis:
  name: ${REDIS_DBNAME}
  config:
    ${REDIS_DBNAME}:
      host: ${REDIS_HOST}
      type: fox_redis
      pool: 1
      timeout: 100
  desc: "type: redis,redis_cluster,fox_redis,fox_redis_cluster"

# Redis object cache. Set enabled=false to bypass all entity caches and fall back to DB.
cache:
  obj:
    enabled: true
