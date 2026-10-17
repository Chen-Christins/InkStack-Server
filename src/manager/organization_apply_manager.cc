#include "organization_apply_manager.h"

#include <chen/log/log.h>

#include "../util.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 500;

OrganizationApplyManager::OrganizationApplyManager()
    : m_cache(8, kCacheMaxSize, 0)
{
}

data::OrganizationApplyInfo::ptr OrganizationApplyManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::OrganizationApplyInfoDao::ParseRow(rt);
}

void OrganizationApplyManager::add(data::OrganizationApplyInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

void OrganizationApplyManager::update(data::OrganizationApplyInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

data::OrganizationApplyInfo::ptr OrganizationApplyManager::get(int64_t id)
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
    v = data::OrganizationApplyInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

int64_t OrganizationApplyManager::listByPages(
    std::vector<data::OrganizationApplyInfo::ptr>& results,
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
    auto qb = data::OrganizationApplyInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("created_at", "DESC");

    int64_t total = 0;
    if (data::OrganizationApplyInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        return 0;
    }
    for (auto& info : results) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return total;
}

int64_t OrganizationApplyManager::listByUserId(
    std::vector<data::OrganizationApplyInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t limit,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationApplyInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("created_at", "DESC");

    int64_t total = 0;
    if (data::OrganizationApplyInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)limit, db)) {
        return 0;
    }
    for (auto& info : results) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return total;
}

} // namespace blog
