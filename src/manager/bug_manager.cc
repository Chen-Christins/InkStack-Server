#include "bug_manager.h"

#include <chen/log/log.h>

#include "../util.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 500;

BugManager::BugManager()
    : m_cache(8, kCacheMaxSize, 0)
{
}

data::BugInfo::ptr BugManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::BugInfoDao::ParseRow(rt);
}

void BugManager::add(data::BugInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

data::BugInfo::ptr BugManager::get(int64_t id)
{
    auto v = m_cache.get(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::BugInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

uint64_t BugManager::listByPages(std::vector<data::BugInfo::ptr>& infos, uint64_t offset, uint64_t size, int32_t status, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::BugInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::BugInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    for (auto& info : infos) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return total;
}

uint64_t BugManager::listByOrg(std::vector<data::BugInfo::ptr>& infos, int64_t orgId, uint64_t offset, uint64_t size, int32_t status, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::BugInfoDao::newQuery();
    qb->where("org_id", "=", orgId);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::BugInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    for (auto& info : infos) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return total;
}

} // namespace blog
