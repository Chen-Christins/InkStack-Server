#include "article_praise_rel_manager.h"

#include "../util.h"
#include "cache_util.h"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 1000;

ArticlePraiseRelManager::ArticlePraiseRelManager()
    : m_cache(8, kCacheMaxSize, 0)
{
}

data::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::ArticlePraiseRelInfoDao::ParseRow(rt);
}

void ArticlePraiseRelManager::add(data::ArticlePraiseRelInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

data::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::get(int64_t id)
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
    v = data::ArticlePraiseRelInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

data::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::getByUserAndArticle(int64_t user_id, int64_t article_id)
{
    std::string ck = "pra:" + std::to_string(user_id) + ":" + std::to_string(article_id);
    int64_t cachedId = getCachedIdMapping(ck);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::ArticlePraiseRelInfoDao::QueryByUserIdArticleId(user_id, article_id, db);
    if (info) {
        m_cache.set(info->getId(), info);
        cacheIdMapping(ck, info->getId());
    }
    return info;
}

data::ArticlePraiseRelInfo::ptr ArticlePraiseRelManager::praise(int64_t user_id, int64_t article_id)
{
    auto existing = getByUserAndArticle(user_id, article_id);
    if (existing) {
        if (!existing->getIsDeleted()) {
            return existing;
        }
        auto db = GetDB();
        if (!db) {
            ERROR(logger) << "Get DB connection fail";
            return nullptr;
        }
        existing->setIsDeleted(0);
        existing->setUpdateTime(time(0));
        if (data::ArticlePraiseRelInfoDao::Update(existing, db)) {
            ERROR(logger) << "ArticlePraiseRelManager praise Update fail";
            return nullptr;
        }
        m_cache.set(existing->getId(), existing);
        return existing;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::ArticlePraiseRelInfo>();
    info->setUserId(user_id);
    info->setArticleId(article_id);
    info->setIsDeleted(0);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::ArticlePraiseRelInfoDao::Insert(info, db)) {
        ERROR(logger) << "ArticlePraiseRelManager praise Insert fail";
        return nullptr;
    }

    m_cache.set(info->getId(), info);
    return info;
}

bool ArticlePraiseRelManager::unpraise(int64_t user_id, int64_t article_id)
{
    auto info = getByUserAndArticle(user_id, article_id);
    if (!info || info->getIsDeleted()) {
        return false;
    }

    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    info->setIsDeleted(1);
    info->setUpdateTime(time(0));
    if (data::ArticlePraiseRelInfoDao::Update(info, db)) {
        ERROR(logger) << "ArticlePraiseRelManager unpraise Update fail";
        return false;
    }
    m_cache.set(info->getId(), info);
    return true;
}

bool ArticlePraiseRelManager::isPraising(int64_t user_id, int64_t article_id)
{
    auto info = getByUserAndArticle(user_id, article_id);
    return info && !info->getIsDeleted();
}

void ArticlePraiseRelManager::listByArticle(std::vector<data::ArticlePraiseRelInfo::ptr>& results, int64_t article_id, uint64_t offset, uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::ArticlePraiseRelInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (data::ArticlePraiseRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    for (auto& info : results) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
}

void ArticlePraiseRelManager::listByUser(std::vector<data::ArticlePraiseRelInfo::ptr>& results, int64_t user_id, uint64_t offset, uint64_t size)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::ArticlePraiseRelInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);
    if (data::ArticlePraiseRelInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    for (auto& info : results) {
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
}

int64_t ArticlePraiseRelManager::countByArticle(int64_t article_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::ArticlePraiseRelInfoDao::newQuery();
    qb->where("article_id", "=", article_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

int64_t ArticlePraiseRelManager::countByUser(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::ArticlePraiseRelInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_deleted", "=", (int64_t)0);
    int64_t total = 0;
    if (qb->executeCount(total, db)) {
        ERROR(logger) << "executeCount fail errno=" << db->getErrno();
        return 0;
    }
    return total;
}

} // namespace blog
