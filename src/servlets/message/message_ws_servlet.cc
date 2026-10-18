#include "message_ws_servlet.h"

#include <chen/log/log.h>
#include <chen/util/json_util.h>
#include <json/json.h>

#include "../../error_codes.h"
#include "../../manager/message_manager.h"
#include "../../manager/user_login_device_manager.h"
#include "../../manager/user_manager.h"
#include "message_json.h"

namespace blog::servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static void sendError(chen::http::WSSession::ptr session, int32_t errno_, const std::string& errstr)
{
    Json::Value err;
    err["errno"] = errno_;
    err["errstr"] = errstr;
    session->sendMessage(chen::JsonUtil::ToString(err));
}

MessageWSServlet::MessageWSServlet()
    : chen::http::WSServlet("MessageWS")
{
}

int32_t MessageWSServlet::onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    std::string uid_str = header->getParam("uid");
    std::string token = header->getParam("token");
    std::string token_time_str = header->getParam("token_time");

    INFO(logger) << "[ChatWS] onConnect: uid=" << uid_str << " token=" << (token.empty() ? "(empty)" : "***") << " token_time=" << token_time_str;

    if (uid_str.empty() || token.empty() || token_time_str.empty()) {
        INFO(logger) << "[ChatWS] onConnect FAIL: missing params";
        sendError(session, errcode::PARAM_MISSING, "Required parameter missing");
        return -1;
    }

    int64_t uid = std::stoll(uid_str);
    int64_t token_time = std::stoll(token_time_str);

    if (token_time <= time(0)) {
        INFO(logger) << "[ChatWS] onConnect FAIL: token expired, token_time=" << token_time << " now=" << time(0);
        sendError(session, errcode::NOT_LOGIN, "Token expired, please re-login");
        return -1;
    }

    data::UserInfo::ptr uinfo = UserMgr::GetInstance()->get(uid);
    if (!uinfo) {
        INFO(logger) << "[ChatWS] onConnect FAIL: user not found uid=" << uid;
        sendError(session, errcode::USER_NOT_FOUND, "User not found");
        return -1;
    }
    if (uinfo->getState() != 1) {
        INFO(logger) << "[ChatWS] onConnect FAIL: user state=" << uinfo->getState() << " uid=" << uid;
        sendError(session, errcode::ACCOUNT_INVALID, "Account status abnormal");
        return -1;
    }
    if (uinfo->getRole() == UserManager::Role::USER) {
        INFO(logger) << "[ChatWS] onConnect FAIL: no permission uid=" << uid;
        sendError(session, errcode::ACCESS_DENIED, "Chat not available for normal users");
        return -1;
    }

    // 以设备表 token 为准校验（Redis 优先，DB 兜底）
    if (!UserLoginDeviceMgr::GetInstance()->validateToken(uid, token, time(0))) {
        INFO(logger) << "[ChatWS] onConnect FAIL: token mismatch, uid=" << uid;
        sendError(session, errcode::NOT_LOGIN, "Token mismatch, please re-login");
        return -1;
    }

    MessageMgr::GetInstance()->addChatConnection(uid, session);
    INFO(logger) << "[ChatWS] onConnect OK: uid=" << uid;
    return 0;
}

int32_t MessageWSServlet::onClose(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    std::string uid_str = header->getParam("uid");
    if (!uid_str.empty()) {
        int64_t uid = std::stoll(uid_str);
        MessageMgr::GetInstance()->removeChatConnection(uid, session);
        INFO(logger) << "[ChatWS] onClose: uid=" << uid;
    }
    return 0;
}

int32_t MessageWSServlet::handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session)
{
    do {
        // 身份解析：优先握手请求里的 uid，其次按连接反查
        int64_t uid = MessageMgr::GetInstance()->getUidBySession(session);
        if (!uid) {
            std::string uid_str = header->getParam("uid");
            if (!uid_str.empty()) {
                uid = std::stoll(uid_str);
            }
        }
        if (!uid) {
            sendError(session, errcode::NOT_LOGIN, "Not logged in");
            break;
        }
        // 兜底：连接鉴权之外的二次校验（防止角色变动后仍持有连接）
        auto user = UserMgr::GetInstance()->get(uid);
        if (!user || user->getRole() == UserManager::Role::USER) {
            sendError(session, errcode::ACCESS_DENIED, "Chat not available for normal users");
            break;
        }

        Json::Value root;
        if (!chen::JsonUtil::FromString(root, msg->getData()) || !root.isObject()) {
            sendError(session, errcode::PARAM_INVALID, "Invalid frame");
            break;
        }
        std::string type = root.get("type", "").asString();

        // 心跳处理
        if (type == "ping") {
            session->sendMessage("{\"type\":\"pong\"}");
            break;
        }

        if (type == "send") {
            std::string client_id = root.get("client_id", "").asString();
            int64_t conversation_id = root.get("conversation_id", 0).asInt64();
            std::string text = root.get("text", "").asString();

            auto sendSendError = [&](int32_t ec, const std::string& errstr) {
                Json::Value err;
                err["type"] = "send_error";
                err["client_id"] = client_id;
                err["errno"] = ec;
                err["errstr"] = errstr;
                session->sendMessage(chen::JsonUtil::ToString(err));
            };

            if (conversation_id <= 0 || text.empty()) {
                sendSendError(errcode::PARAM_MISSING, "conversation_id and text required");
                break;
            }
            if (text.size() > MessageManager::kMaxMessageLen) {
                sendSendError(errcode::MESSAGE_TEXT_TOO_LONG, "Message too long");
                break;
            }

            auto conv = MessageMgr::GetInstance()->get(conversation_id);
            if (!conv) {
                sendSendError(errcode::MESSAGE_CONVERSATION_NOT_FOUND, "Conversation not found");
                break;
            }
            if (uid != conv->getUserAId() && uid != conv->getUserBId()) {
                sendSendError(errcode::ACCESS_DENIED, "Access denied");
                break;
            }
            int64_t peer_id = (conv->getUserAId() == uid) ? conv->getUserBId() : conv->getUserAId();

            auto msg_info = MessageMgr::GetInstance()->sendMessage(conversation_id, uid, text);
            if (!msg_info) {
                sendSendError(errcode::MESSAGE_SEND_FAILED, "Send failed");
                break;
            }
            INFO(logger) << "[ChatWS] send OK: uid=" << uid << " conv=" << conversation_id << " msg_id=" << msg_info->getId()
                         << " client_id=" << client_id;

            // ACK 给发送方（携带 client_id 供前端对齐乐观消息，recipient_online 供送达状态）
            {
                Json::Value ack;
                ack["type"] = "message_ack";
                ack["client_id"] = client_id;
                ack["conversation_id"] = conversation_id;
                Json::Value mj;
                BuildMessageJson(mj, msg_info, uid);
                ack["message"] = mj;
                ack["recipient_online"] = MessageMgr::GetInstance()->isChatConnected(peer_id);
                MessageMgr::GetInstance()->sendToUser(uid, chen::JsonUtil::ToString(ack));
            }

            // 推送给接收方
            if (MessageMgr::GetInstance()->isChatConnected(peer_id)) {
                Json::Value push;
                push["type"] = "message";
                push["conversation_id"] = conversation_id;
                Json::Value mj;
                BuildMessageJson(mj, msg_info, peer_id);
                push["message"] = mj;
                MessageMgr::GetInstance()->sendToUser(peer_id, chen::JsonUtil::ToString(push));
            }
            INFO(logger) << "[ChatWS] ack->" << uid << " push->" << peer_id << " peer_online=" << MessageMgr::GetInstance()->isChatConnected(peer_id);
        } else if (type == "read") {
            int64_t conversation_id = root.get("conversation_id", 0).asInt64();
            if (conversation_id <= 0) {
                sendError(session, errcode::PARAM_MISSING, "conversation_id required");
                break;
            }

            auto conv = MessageMgr::GetInstance()->get(conversation_id);
            if (!conv) {
                sendError(session, errcode::MESSAGE_CONVERSATION_NOT_FOUND, "Conversation not found");
                break;
            }
            if (uid != conv->getUserAId() && uid != conv->getUserBId()) {
                sendError(session, errcode::ACCESS_DENIED, "Access denied");
                break;
            }

            MessageMgr::GetInstance()->markRead(conversation_id, uid);

            // 已读回执给对端
            int64_t peer_id = (conv->getUserAId() == uid) ? conv->getUserBId() : conv->getUserAId();
            if (MessageMgr::GetInstance()->isChatConnected(peer_id)) {
                Json::Value rcpt;
                rcpt["type"] = "read";
                rcpt["conversation_id"] = conversation_id;
                rcpt["unread"] = 0;
                MessageMgr::GetInstance()->sendToUser(peer_id, chen::JsonUtil::ToString(rcpt));
            }
        } else {
            sendError(session, errcode::PARAM_INVALID, "Unsupported frame type");
        }
    } while (0);

    return 0;
}

} // namespace blog::servlet