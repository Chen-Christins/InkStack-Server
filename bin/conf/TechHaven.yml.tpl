# 业务配置

# system default ai model for article summary
ai:
  type: glm
  model: glm-4.7-flash
  api_key: # system default AI API key, used when user has not configured their own AI settings

# 默认超级管理员：服务启动时若不存在管理员则自动创建，用于首次登录并配置 SMTP 等系统设置
admin:
  account: admin
  passwd: admin123456
  email: admin@example.com

# 分词字典配置
search:
  jieba_dict_path: ${WORK_PATH}/dict
  index_path: ${WORK_PATH}/search_index.dat

# 第三方登录配置
oauth:
  github:
    client_id: ${GITHUB_CLIENT_ID}
    client_secret: ${GITHUB_CLIENT_SECRET}
    # 必须与 GitHub OAuth App 的 Authorization callback URL 完全一致（经前端同源代理）
    callback_url: ${GITHUB_CALLBACK_URL}
    # 授权成功后回跳的前端站点根地址
    frontend_base: ${FRONTEND_BASE}

# Server configuration, including work path, PID file, and email service settings for sending notifications or alerts.
server:
  work_path: ${WORK_PATH}
  data_path: ${DATA_PATH}
  pid_file: server.pid