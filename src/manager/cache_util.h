/**
 * @file cache_util.h
 * @brief Redis 缓存工具（COUNT 缓存、列表结果缓存）
 * @author Christins
 * @date 2026-06-06
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/db/query_builder.h>

namespace blog {

/// COUNT 缓存 TTL（秒）
static const int kCountCacheTTL = 30;
/// 列表结果缓存 TTL（秒）
static const int kListCacheTTL = 10;
/// Stats 缓存 TTL（秒）
static const int kStatsCacheTTL = 60;
/// ID 映射缓存 TTL（秒）— alternate_key → id
static const int kIdMapCacheTTL = 300;

/**
 * @brief 执行 COUNT 查询，优先从 Redis 缓存读取
 * @param qb QueryBuilder（已配置好 WHERE/JOIN）
 * @param conn 数据库连接
 * @param cacheKey 缓存键（不含前缀）
 * @param ttlSec 缓存 TTL 秒数
 * @return COUNT 结果，失败返回 0
 */
int64_t executeCountCached(chen::QueryBuilder::ptr qb, chen::IDB::ptr conn, const std::string& cacheKey, int ttlSec = kCountCacheTTL);

/**
 * @brief 缓存分页列表结果（ID 数组）
 * @param cacheKey 缓存键
 * @param ids 文章/评论 ID 列表
 * @param ttlSec TTL 秒数
 */
void cacheListResult(const std::string& cacheKey, const std::vector<int64_t>& ids, int ttlSec = kListCacheTTL);

/**
 * @brief 读取缓存的分页列表结果
 * @param cacheKey 缓存键
 * @param[out] ids 解析后的 ID 列表
 * @return true 表示命中缓存
 */
bool getCachedListResult(const std::string& cacheKey, std::vector<int64_t>& ids);

/**
 * @brief 缓存字符串结果
 * @param cacheKey 缓存键
 * @param value 字符串值
 * @param ttlSec TTL 秒数
 */
void cacheStringResult(const std::string& cacheKey, const std::string& value, int ttlSec = kStatsCacheTTL);

/**
 * @brief 读取缓存的字符串结果
 * @param cacheKey 缓存键
 * @param[out] value 缓存的字符串
 * @return true 表示命中缓存
 */
bool getCachedStringResult(const std::string& cacheKey, std::string& value);

/**
 * @brief 缓存 alternate_key → id 映射
 * @param cacheKey 缓存键（不含前缀）
 * @param id 主键 ID
 * @param ttlSec TTL 秒数
 */
void cacheIdMapping(const std::string& cacheKey, int64_t id, int ttlSec = kIdMapCacheTTL);

/**
 * @brief 读取 alternate_key → id 映射
 * @param cacheKey 缓存键（不含前缀）
 * @return ID，未命中返回 0
 */
int64_t getCachedIdMapping(const std::string& cacheKey);

} // namespace blog
