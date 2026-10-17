#include "category_manager.h"

#include <chen/log/log.h>

#include "../util.h"
#include "cache_util.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 300;

CategoryManager::CategoryManager()
    : m_cache(4, kCacheMaxSize, 30)
{
}

data::CategoryInfo::ptr CategoryManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::CategoryInfoDao::ParseRow(rt);
}

void CategoryManager::add(blog::data::CategoryInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

blog::data::CategoryInfo::ptr CategoryManager::get(int64_t id)
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
    v = data::CategoryInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

void CategoryManager::listAll(std::vector<blog::data::CategoryInfo::ptr>& infos, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::CategoryInfoDao::newQuery();
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "ASC");
    if (data::CategoryInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    for (auto& info : infos) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
}

blog::data::CategoryInfo::ptr CategoryManager::getByName(const std::string& name)
{
    int64_t cachedId = getCachedIdMapping("cat:name:" + name);
    if (cachedId > 0) {
        return get(cachedId);
    }
    // 先扫缓存
    // LRU 缓存只支持按 id 查找，所以直接用 DAO 查 DB
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::CategoryInfoDao::QueryByName(name, db);
    if (info) {
        m_cache.set(info->getId(), info);
        cacheIdMapping("cat:name:" + name, info->getId());
    }
    return info;
}

} // namespace blog
