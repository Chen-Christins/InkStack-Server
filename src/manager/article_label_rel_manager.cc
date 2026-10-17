#include "article_label_rel_manager.h"

#include "../util.h"
#include "cache_util.h"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 500;

ArticleLabelRelManager::ArticleLabelRelManager()
    : m_cache(8, kCacheMaxSize, 0)
{
}

data::ArticleLabelRelInfo::ptr ArticleLabelRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::ArticleLabelRelInfoDao::ParseRow(rt);
}

void ArticleLabelRelManager::add(data::ArticleLabelRelInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

data::ArticleLabelRelInfo::ptr ArticleLabelRelManager::get(int64_t id)
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
    v = data::ArticleLabelRelInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

bool ArticleLabelRelManager::listByArticleId(std::vector<data::ArticleLabelRelInfo::ptr>& infos, int64_t id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::ArticleLabelRelInfoDao::newQuery();
    qb->where("article_id", "=", id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    for (auto& info : infos) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return true;
}

bool ArticleLabelRelManager::listByLabelId(std::vector<data::ArticleLabelRelInfo::ptr>& infos, int64_t label_id, bool valid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }
    auto qb = data::ArticleLabelRelInfoDao::newQuery();
    qb->where("label_id", "=", label_id);
    qb->whereIf(valid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    if (data::ArticleLabelRelInfoDao::QueryByBuilder(infos, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return false;
    }
    for (auto& info : infos) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return true;
}

data::ArticleLabelRelInfo::ptr ArticleLabelRelManager::getByArticleIdLabelId(int64_t article_id, int64_t label_id)
{
    std::string ck = "alr:" + std::to_string(article_id) + ":" + std::to_string(label_id);
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::ArticleLabelRelInfoDao::QueryByArticleIdLabelId(article_id, label_id, db);
    if (info) {
        m_cache.set(info->getId(), info);
        cacheIdMapping(ck, info->getId());
    }
    return info;
}

} // namespace blog
