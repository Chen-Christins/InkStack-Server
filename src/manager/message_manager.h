/**
 * @file message_manager.h
 * @brief 私信会话与消息管理（会话、消息、未读数、已读标记）
 * @author Christins
 * @date 2026-09-01
 * @copyright Apache 2.0
 */
#pragma once

#include <chen/ds/lru_cache.h>
#include <chen/http/ws_session.h>
#include <chen/util/singleton.h>

#include "blog/data/conversation_info.h"
#include "blog/data/conversation_message_info.h"

#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace blog {

class MessageManager {
public:
    /// 单条消息最大长度
    static const size_t kMaxMessageLen = 2000;

    MessageManager();

    /// 会话相关
    data::ConversationInfo::ptr get(int64_t id);

    data::ConversationInfo::ptr getByUsers(int64_t uid_a, int64_t uid_b);

    data::ConversationInfo::ptr getOrCreate(int64_t uid_a, int64_t uid_b);

    void listConversations(std::vector<data::ConversationInfo::ptr>& results, int64_t uid);

    /// 消息相关
    void listMessages(std::vector<data::ConversationMessageInfo::ptr>& results, int64_t conversation_id, int32_t offset, int32_t limit);

    data::ConversationMessageInfo::ptr sendMessage(int64_t conversation_id, int64_t sender_id, const std::string& text);

    bool markRead(int64_t conversation_id, int64_t uid);

    /// 在本端隐藏会话（不影响对方与消息数据）
    bool deleteForUser(int64_t conversation_id, int64_t uid);

    /// 聊天 WS 连接管理（标准双向通道，连接即在线；支持多标签页/多设备）
    void addChatConnection(int64_t user_id, chen::http::WSSession::ptr session);

    void removeChatConnection(int64_t user_id, chen::http::WSSession::ptr session);

    bool isChatConnected(int64_t user_id);

    int32_t sendToUser(int64_t user_id, const std::string& message);

    void closeAllChatConnections();

    /// 通过连接反查用户（每帧消息需要知道发送者身份）
    int64_t getUidBySession(chen::http::WSSession::ptr session);

    /// 参与者归一化：user_a_id 恒为较小值
    static int64_t NormalizeUserA(int64_t uid_a, int64_t uid_b) { return uid_a < uid_b ? uid_a : uid_b; }

    static int64_t NormalizeUserB(int64_t uid_a, int64_t uid_b) { return uid_a < uid_b ? uid_b : uid_a; }

private:
    /// 会话缓存
    chen::ds::HashLruCache<int64_t, data::ConversationInfo::ptr> m_cache;

    /// 聊天 WS 连接: user_id -> 该用户的所有在线连接
    std::unordered_map<int64_t, std::vector<chen::http::WSSession::ptr>> m_chatConnections;
    std::shared_mutex m_chatMutex;
};

typedef chen::Singleton<MessageManager> MessageMgr;

} // namespace blog
