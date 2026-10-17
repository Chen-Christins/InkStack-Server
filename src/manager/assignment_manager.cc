#include "assignment_manager.h"

#include "../util.h"
#include "cache_util.h"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 200;

AssignmentManager::AssignmentManager()
    : m_cache(4, kCacheMaxSize, 0)
{
}

data::AssignmentInfo::ptr AssignmentManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::AssignmentInfoDao::ParseRow(rt);
}

void AssignmentManager::add(data::AssignmentInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

data::AssignmentInfo::ptr AssignmentManager::get(int64_t id)
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
    v = data::AssignmentInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

data::AssignmentInfo::ptr AssignmentManager::getByName(const std::string& subject_name, const std::string& name)
{
    std::string ck = "assign:name:" + subject_name + ":" + name;
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    std::vector<data::AssignmentInfo::ptr> results;
    if (data::AssignmentInfoDao::QueryBySubjectNameName(results, subject_name, name, db)) {
        return nullptr;
    }
    if (!results.empty()) {
        m_cache.set(results[0]->getId(), results[0]);
        cacheIdMapping(ck, results[0]->getId());
        return results[0];
    }
    return nullptr;
}

uint64_t AssignmentManager::listByPages(std::vector<data::AssignmentInfo::ptr>& infos, uint64_t offset, uint64_t size, int32_t status, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::AssignmentInfoDao::newQuery();
    qb->whereIf(status != -1, "status", "=", (int64_t)status);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::AssignmentInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    for (auto& info : infos) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return total;
}

AssignmentManager::AssignmentStats AssignmentManager::getStats()
{
    AssignmentStats stats;
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return stats;
    }

    // Query status counts with GROUP BY
    {
        auto qb = data::AssignmentInfoDao::newQuery();
        qb->select("status, COUNT(*) AS cnt");
        qb->where("is_deleted", "=", (int64_t)0);
        qb->groupBy("status");
        std::vector<std::pair<int32_t, int64_t>> rows;
        if (qb->queryPairs<int32_t, int64_t>(rows, db) == 0) {
            for (auto& [s, cnt] : rows) {
                stats.total += cnt;
                switch (s) {
                case Status::ACTIVE:
                    stats.active = cnt;
                    break;
                case Status::INACTIVE:
                    stats.closed = cnt;
                    break;
                case Status::DRAFT:
                    stats.draft = cnt;
                    break;
                }
            }
        }
    }

    return stats;
}

} // namespace blog
