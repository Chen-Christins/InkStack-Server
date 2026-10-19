#pragma once

#include <chen/db/query_builder.h>
#include <chen/http/ws_session.h>
#include <chen/util/singleton.h>

#include <shared_mutex>
#include <unordered_map>

#include "blog/data/notification_info.h"

namespace blog {

struct NotificationData {
    int64_t user_id = 0;
    std::string title;
    std::string content;
    std::string type;
    int64_t sender_id = 0;
    int64_t article_id = 0;
    int64_t comment_id = 0;
    int32_t is_broadcast = 0;
    std::string level = "";
    int64_t start_time = 0;
    int64_t end_time = 0;
};

class NotificationManager {
public:
    // WS connection management
    void addConnection(int64_t user_id, chen::http::WSSession::ptr session);

    void removeConnection(int64_t user_id);

    void closeAllConnections();

    int32_t sendToUser(int64_t user_id, const std::string& message);

    void broadcast(const std::string& message);

    bool isConnected(int64_t user_id);

    int32_t getOnlineCount();

    // Presence WS connection management
    void addPresenceConnection(int64_t user_id, chen::http::WSSession::ptr session);

    void removePresenceConnection(int64_t user_id);

    void broadcastPresence(const std::string& message);

    int32_t getPresenceOnlineCount();

    bool isPresenceConnected(int64_t user_id);

    // DB persistence
    data::NotificationInfo::ptr get(int64_t id);

    data::NotificationInfo::ptr addNotification(const NotificationData& data);

    void update(data::NotificationInfo::ptr info);

    void listByUser(std::vector<data::NotificationInfo::ptr>& results, int64_t user_id, uint64_t offset, uint64_t size, const std::string& type = "");

    int64_t countByUser(int64_t user_id, const std::string& type = "");

    int64_t unreadCount(int64_t user_id);

    bool markRead(int64_t notification_id);

    bool markRead(const std::vector<int64_t>& ids);

    int64_t markAllRead(int64_t user_id);

    int64_t markReadByType(int64_t user_id, const std::string& type);

    void cleanupExpiredBroadcasts();

private:
    static data::NotificationInfo::ptr parseRow(chen::ISQLData::ptr rt);

    // WS connections (notification)
    std::unordered_map<int64_t, chen::http::WSSession::ptr> m_connections;
    std::shared_mutex m_connMutex;

    // WS connections (presence)
    std::unordered_map<int64_t, chen::http::WSSession::ptr> m_presenceConnections;
    std::shared_mutex m_presenceMutex;
};

typedef chen::Singleton<NotificationManager> NotificationMgr;

} // namespace blog
