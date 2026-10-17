#include "organization_manager.h"

#include "../util.h"
#include "cache_util.h"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 300;

OrganizationManager::OrganizationManager()
    : m_cache(4, kCacheMaxSize, 30)
{
}

data::OrganizationInfo::ptr OrganizationManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::OrganizationInfoDao::ParseRow(rt);
}

void OrganizationManager::add(data::OrganizationInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

data::OrganizationInfo::ptr OrganizationManager::get(int64_t id)
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
    v = data::OrganizationInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

data::OrganizationInfo::ptr OrganizationManager::getByName(const std::string& name)
{
    int64_t cachedId = getCachedIdMapping("org:name:" + name);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::OrganizationInfoDao::QueryByName(name, db);
    if (info) {
        m_cache.set(info->getId(), info);
        cacheIdMapping("org:name:" + name, info->getId());
    }
    return info;
}

int64_t OrganizationManager::listByPages(
    std::vector<data::OrganizationInfo::ptr>& orgs,
    uint64_t offset,
    uint64_t limit,
    int32_t status,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::OrganizationInfoDao::QueryByBuilderPages(orgs, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        return 0;
    }
    for (auto& info : orgs) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return total;
}

OrganizationManager::OrganizationStats OrganizationManager::getStats()
{
    OrganizationStats stats;

    std::string cached;
    if (getCachedStringResult("org:stats", cached)) {
        std::stringstream ss(cached);
        std::string token;
        auto next = [&]() -> int64_t {
            std::getline(ss, token, '|');
            return chen::TypeUtil::Atoi(token);
        };
        stats.total = next();
        stats.active = next();
        stats.inactive = next();
        return stats;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    // Query total (non-deleted)
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        int64_t total = 0;
        if (qb->executeCount(total, db) == 0) {
            stats.total = total;
        }
    }

    // Query active
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        qb->where("status", "=", (int64_t)Status::ACTIVE);
        int64_t active = 0;
        if (qb->executeCount(active, db) == 0) {
            stats.active = active;
        }
    }

    // Query inactive
    {
        auto qb = data::OrganizationInfoDao::newQuery();
        qb->where("is_deleted", "=", (int64_t)0);
        qb->where("status", "=", (int64_t)Status::INACTIVE);
        int64_t inactive = 0;
        if (qb->executeCount(inactive, db) == 0) {
            stats.inactive = inactive;
        }
    }

    std::stringstream ss;
    ss << stats.total << "|" << stats.active << "|" << stats.inactive;
    cacheStringResult("org:stats", ss.str());

    return stats;
}

} // namespace blog
