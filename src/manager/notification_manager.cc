#include "notification_manager.h"

#include "../util.h"
#include "cache_util.h"

#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 2000;

NotificationManager::NotificationManager()
    : m_cache(16, kCacheMaxSize, 0)
{
}

data::NotificationInfo::ptr NotificationManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::NotificationInfoDao::ParseRow(rt);
}

// ========== WS connection management ==========

void NotificationManager::addConnection(int64_t user_id, chen::http::WSSession::ptr session)
{
    std::unique_lock lock(m_connMutex);
    m_connections[user_id] = session;
    INFO(logger) << "Notification WS connected: user_id=" << user_id;
}

void NotificationManager::removeConnection(int64_t user_id)
{
    std::unique_lock lock(m_connMutex);
    m_connections.erase(user_id);
    INFO(logger) << "Notification WS disconnected: user_id=" << user_id;
}

void NotificationManager::closeAllConnections()
{
    {
        std::unique_lock lock(m_connMutex);
        for (auto& [user_id, session] : m_connections) {
            session->close();
            INFO(logger) << "Notification WS closed: user_id=" << user_id;
        }
        m_connections.clear();
        INFO(logger) << "All Notification WS connections closed";
    }
    {
        std::unique_lock lock(m_presenceMutex);
        for (auto& [user_id, session] : m_presenceConnections) {
            session->close();
            INFO(logger) << "Presence WS closed: user_id=" << user_id;
        }
        m_presenceConnections.clear();
        INFO(logger) << "All Presence WS connections closed";
    }
}

int32_t NotificationManager::sendToUser(int64_t user_id, const std::string& message)
{
    std::shared_lock lock(m_connMutex);
    auto it = m_connections.find(user_id);
    if (it == m_connections.end()) {
        return -1;
    }
    return it->second->sendMessage(message);
}

void NotificationManager::broadcast(const std::string& message)
{
    std::shared_lock lock(m_connMutex);
    for (auto& [user_id, session] : m_connections) {
        session->sendMessage(message);
    }
}

bool NotificationManager::isConnected(int64_t user_id)
{
    std::shared_lock lock(m_connMutex);
    return m_connections.find(user_id) != m_connections.end();
}

int32_t NotificationManager::getOnlineCount()
{
    std::shared_lock lock(m_connMutex);
    return (int32_t)m_connections.size();
}

// ========== Presence WS connection management ==========

void NotificationManager::addPresenceConnection(int64_t user_id, chen::http::WSSession::ptr session)
{
    std::unique_lock lock(m_presenceMutex);
    m_presenceConnections[user_id] = session;
    INFO(logger) << "Presence WS connected: user_id=" << user_id;
}

void NotificationManager::removePresenceConnection(int64_t user_id)
{
    std::unique_lock lock(m_presenceMutex);
    m_presenceConnections.erase(user_id);
    INFO(logger) << "Presence WS disconnected: user_id=" << user_id;
}

void NotificationManager::broadcastPresence(const std::string& message)
{
    std::shared_lock lock(m_presenceMutex);
    for (auto& [user_id, session] : m_presenceConnections) {
        session->sendMessage(message);
    }
}

int32_t NotificationManager::getPresenceOnlineCount()
{
    std::shared_lock lock(m_presenceMutex);
    return (int32_t)m_presenceConnections.size();
}

bool NotificationManager::isPresenceConnected(int64_t user_id)
{
    std::shared_lock lock(m_presenceMutex);
    return m_presenceConnections.find(user_id) != m_presenceConnections.end();
}

// ========== DB persistence ==========

data::NotificationInfo::ptr NotificationManager::addNotification(const NotificationData& data)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }

    auto info = std::make_shared<data::NotificationInfo>();
    info->setUserId(data.user_id);
    info->setTitle(data.title);
    info->setContent(data.content);
    info->setType(data.type);
    info->setSenderId(data.sender_id);
    info->setArticleId(data.article_id);
    info->setCommentId(data.comment_id);
    info->setIsRead(0);
    info->setReadTime(0);
    info->setIsDeleted(0);
    info->setIsBroadcast(data.is_broadcast);
    info->setLevel(data.level);
    info->setStartTime(data.start_time);
    info->setEndTime(data.end_time);
    info->setCreateTime(time(0));
    info->setUpdateTime(time(0));

    if (data::NotificationInfoDao::Insert(info, db)) {
        ERROR(logger) << "NotificationManager addNotification Insert fail";
        return nullptr;
    }

    m_cache.set(info->getId(), info);
    return info;
}

void NotificationManager::listByUser(
    std::vector<data::NotificationInfo::ptr>& results,
    int64_t user_id,
    uint64_t offset,
    uint64_t size,
    const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(!type.empty(), "type", "=", type);
    qb->orderBy("id", "DESC");
    qb->limit((int32_t)size);
    qb->offset((int32_t)offset);

    // 结果缓存：仅对首页做缓存
    std::string typeKey = type.empty() ? "all" : type;
    std::string listKey = "notif:list:" + std::to_string(user_id) + ":" + typeKey + ":" + std::to_string(offset) + ":" + std::to_string(size);
    std::vector<int64_t> cachedIds;
    if (getCachedListResult(listKey, cachedIds)) {
        for (auto id : cachedIds) {
            auto info = get(id);
            if (info) {
                results.push_back(info);
            }
        }
        return;
    }

    if (data::NotificationInfoDao::QueryByBuilder(results, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return;
    }
    std::vector<int64_t> ids;
    for (auto& info : results) {
        ids.push_back(info->getId());
        if (!m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    cacheListResult(listKey, ids);
}

int64_t NotificationManager::countByUser(int64_t user_id, const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->whereIf(!type.empty(), "type", "=", type);
    std::string ck = "notif:cnt:" + std::to_string(user_id) + ":" + (type.empty() ? "all" : type);
    return executeCountCached(qb, db, ck);
}

int64_t NotificationManager::unreadCount(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    return executeCountCached(qb, db, "notif:unread:" + std::to_string(user_id));
}

bool NotificationManager::markRead(int64_t notification_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    auto info = get(notification_id);
    if (!info) {
        return false;
    }

    info->setIsRead(1);
    info->setReadTime(time(0));
    if (data::NotificationInfoDao::Update(info, db)) {
        return false;
    }
    return true;
}

bool NotificationManager::markRead(const std::vector<int64_t>& ids)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return false;
    }

    for (auto id : ids) {
        auto info = get(id);
        if (!info) {
            continue;
        }
        info->setIsRead(1);
        info->setReadTime(time(0));
        data::NotificationInfoDao::Update(info, db);
    }
    return true;
}

int64_t NotificationManager::markAllRead(int64_t user_id)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    // Find all unread, non-deleted notifications for this user
    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    std::vector<data::NotificationInfo::ptr> unread;
    if (data::NotificationInfoDao::QueryByBuilder(unread, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }

    int64_t count = 0;
    for (auto& info : unread) {
        info->setIsRead(1);
        info->setReadTime(time(0));
        if (data::NotificationInfoDao::Update(info, db) == 0) {
            m_cache.set(info->getId(), info);
            count++;
        }
    }
    return count;
}

int64_t NotificationManager::markReadByType(int64_t user_id, const std::string& type)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    auto qb = data::NotificationInfoDao::newQuery();
    qb->where("user_id", "=", user_id);
    qb->where("is_read", "=", (int64_t)0);
    qb->where("is_deleted", "=", (int64_t)0);
    qb->where("type", "=", type);
    std::vector<data::NotificationInfo::ptr> unread;
    if (data::NotificationInfoDao::QueryByBuilder(unread, qb, db)) {
        ERROR(logger) << "QueryByBuilder failed";
        return 0;
    }

    int64_t count = 0;
    for (auto& info : unread) {
        info->setIsRead(1);
        info->setReadTime(time(0));
        if (data::NotificationInfoDao::Update(info, db) == 0) {
            m_cache.set(info->getId(), info);
            count++;
        }
    }
    return count;
}

data::NotificationInfo::ptr NotificationManager::get(int64_t id)
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
    v = data::NotificationInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

void NotificationManager::cleanupExpiredBroadcasts()
{
    auto db = GetDB();
    if (!db) {
        return;
    }
    int64_t now = time(0);
    // 将已过期的广播的 is_broadcast 置 0，前端 /broadcast/list 不再返回
    auto qb = data::NotificationInfoDao::newQuery();
    qb->set("is_broadcast", (int64_t)0);
    qb->set("update_time", now);
    qb->where("is_broadcast", "=", (int64_t)1);
    qb->whereSQL("end_time > ?", (int64_t)0);
    qb->where("end_time", "<=", now);
    auto stmt = db->prepare(qb->buildUpdateSQL());
    if (!stmt) {
        return;
    }
    qb->bindUpdateParams(stmt);
    stmt->execute();
}

} // namespace blog
