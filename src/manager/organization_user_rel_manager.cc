#include "organization_user_rel_manager.h"

#include "../util.h"
#include "cache_util.h"
#include "organization_manager.h"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 1000;

OrganizationUserRelManager::OrganizationUserRelManager()
    : m_cache(8, kCacheMaxSize, 0)
{
}

data::OrganizationUserRelInfo::ptr OrganizationUserRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::OrganizationUserRelInfoDao::ParseRow(rt);
}

void OrganizationUserRelManager::add(data::OrganizationUserRelInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

data::OrganizationUserRelInfo::ptr OrganizationUserRelManager::get(int64_t id)
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
    v = data::OrganizationUserRelInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

data::OrganizationUserRelInfo::ptr OrganizationUserRelManager::getByOrgAndUser(int64_t o_id, int64_t u_id)
{
    int64_t cachedId = getCachedIdMapping("org_usr:" + std::to_string(o_id) + ":" + std::to_string(u_id));
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::OrganizationUserRelInfoDao::QueryByOrgIdUserId(o_id, u_id, db);
    if (info) {
        m_cache.set(info->getId(), info);
        cacheIdMapping("org_usr:" + std::to_string(o_id) + ":" + std::to_string(u_id), info->getId());
    }
    return info;
}

int64_t OrganizationUserRelManager::getByPages(
    std::vector<data::OrganizationUserRelInfo::ptr>& results,
    int64_t o_id,
    uint64_t offset,
    uint64_t size,
    int32_t status,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationUserRelInfoDao::newQuery();
    qb->where("org_id", "=", o_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::OrganizationUserRelInfoDao::QueryByBuilderPages(results, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    for (auto& info : results) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return total;
}

int64_t OrganizationUserRelManager::getOrgByUserId(
    std::vector<data::OrganizationUserRelInfo::ptr>& results,
    int64_t u_id,
    int32_t status,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationUserRelInfoDao::newQuery();
    qb->where("user_id", "=", u_id);
    qb->where("status", "!=", (int64_t)Status::PENDING);
    qb->where("status", "!=", (int64_t)Status::EXITED);
    qb->where("status", "!=", (int64_t)Status::REJECTED);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::OrganizationUserRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }
    for (auto& info : results) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return results.size();
}

int64_t OrganizationUserRelManager::getMemberCount(int64_t o_id, int32_t status, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::OrganizationUserRelInfoDao::newQuery();
    qb->where("org_id", "=", o_id);
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

OrganizationUserRelManager::Stats OrganizationUserRelManager::getStats(int64_t org_id)
{
    Stats stats;
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    // total_members: 所有已批准且未删除的成员
    stats.total_members = getMemberCount(org_id, Status::APPROVED, true);

    // 按角色 GROUP BY 拿到 org_admin_count 和 regular_count
    {
        auto qb = data::OrganizationUserRelInfoDao::newQuery();
        qb->select("role, COUNT(*) as cnt");
        qb->where("org_id", "=", org_id);
        qb->where("status", "=", (int64_t)Status::APPROVED);
        qb->where("is_deleted", "=", (int64_t)0);
        qb->groupBy("role");
        std::vector<std::pair<int32_t, int64_t>> rows;
        if (qb->queryPairs<int32_t, int64_t>(rows, db) == 0) {
            for (auto& [role, cnt] : rows) {
                if (role == OrganizationManager::Role::ORG_ADMIN) {
                    stats.org_admin_count = cnt;
                } else if (role == OrganizationManager::Role::MEMBER) {
                    stats.regular_count = cnt;
                }
            }
        }
    }

    // active_members: 已批准成员中，其用户账号状态为正常的数量
    {
        auto qb = data::OrganizationUserRelInfoDao::newQuery("r");
        qb->join("INNER", "user u", "r.user_id = u.id");
        qb->where("r.org_id", "=", org_id);
        qb->where("r.status", "=", (int64_t)Status::APPROVED);
        qb->where("r.is_deleted", "=", (int64_t)0);
        qb->where("u.state", "=", (int64_t)1);
        qb->executeCount(stats.active_members, db);
    }

    return stats;
}

} // namespace blog
