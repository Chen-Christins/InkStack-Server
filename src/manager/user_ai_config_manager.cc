/**
 * @file user_ai_config_manager.cc
 * @brief 用户AI配置管理器实现
 * @author Christins
 * @date 2026-06-09
 * @copyright Apache 2.0
 */
#include "user_ai_config_manager.h"

#include <chen/log/log.h>

#include "../util.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 200;

UserAIConfigManager::UserAIConfigManager()
    : m_cache(32, kCacheMaxSize, 0)
{
}

blog::data::UserAiConfigInfo::ptr UserAIConfigManager::getByUserId(int64_t user_id)
{
    auto v = m_cache.get(user_id);
    if (v) {
        return v;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    v = data::UserAiConfigInfoDao::QueryByUserId(user_id, db);
    if (v) {
        m_cache.set(user_id, v);
    }
    return v;
}

bool UserAIConfigManager::save(blog::data::UserAiConfigInfo::ptr info)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    info->setUpdateTime(time(0));
    if (!info->getId()) {
        info->setCreateTime(time(0));
    }

    if (data::UserAiConfigInfoDao::InsertOrUpdate(info, db)) {
        ERROR(logger) << "UserAIConfigManager save fail";
        return false;
    }

    // save 之后需要重新查询以获取 DB 生成的 id
    auto saved = data::UserAiConfigInfoDao::QueryByUserId(info->getUserId(), db);
    if (saved) {
        m_cache.set(saved->getUserId(), saved);
    }
    return true;
}

} // namespace blog
