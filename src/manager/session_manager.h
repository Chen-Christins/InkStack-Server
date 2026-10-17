/**
 * @file session_manager.h
 * @brief 会话 Redis 持久化工具 — 内存 + Redis 双存储，支持进程重启后自动恢复
 * @author Christins
 * @date 2026-06-30
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/http/session_data.h>

#include <string>

namespace blog {

/**
 * @brief 获取 session TTL（秒），优先读取系统设置，默认 24 小时
 */
int64_t GetSessionTTL();

/**
 * @brief 保存会话到 Redis Hash，并设置 TTL
 * @param session_id 会话 ID
 * @param uid 用户 ID
 * @param is_auth 是否已尝试验证
 */
void SaveSessionToRedis(const std::string& session_id, int64_t uid, int32_t is_auth);

/**
 * @brief 从 Redis Hash 恢复会话到内存
 * @param session_id 会话 ID
 * @return 恢复的 SessionData 指针，Redis 中无数据或 uid=0 时返回 nullptr
 * @details 恢复成功后自动注册到 SessionDataMgr 并刷新 Redis TTL
 */
chen::http::SessionData::ptr LoadSessionFromRedis(const std::string& session_id);

/**
 * @brief 从 Redis 删除会话数据
 * @param session_id 会话 ID
 */
void DeleteSessionFromRedis(const std::string& session_id);

} // namespace blog
