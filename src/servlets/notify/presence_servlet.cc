#include "presence_servlet.h"

#include "../../error_codes.h"
#include "../../manager/notification_manager.h"
#include "../../manager/user_login_device_manager.h"
#include "../../manager/user_manager.h"

#include <chen/log/log.h>
#include <json/json.h>

namespace blog::servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static void sendError(chen::http::WSSession::ptr session, int32_t errno_, const std::string& errstr)
{
    Json::Value err;
    err["errno"] = errno_;
    err["errstr"] = errstr;
    session->sendMessage(chen::JsonUtil::ToString(err));
}

PresenceServlet::PresenceServlet()
    : chen::http::WSServlet("Presence")
{
}

static void broadcastOnlineCount()
{
    int32_t count = NotificationMgr::GetInstance()->getPresenceOnlineCount();
    Json::Value msg;
    msg["type"] = "online_count";
    msg["count"] = count;
    NotificationMgr::GetInstance()->broadcastPresence(chen::JsonUtil::ToString(msg));
}

int32_t PresenceServlet::onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    std::string uid_str = header->getParam("uid");
    std::string token = header->getParam("token");
    std::string token_time_str = header->getParam("token_time");

    INFO(logger) << "[Presence] onConnect: uid=" << uid_str << " token=" << (token.empty() ? "(empty)" : "***") << " token_time=" << token_time_str;

    if (uid_str.empty() || token.empty() || token_time_str.empty()) {
        INFO(logger) << "[Presence] onConnect FAIL: missing params";
        sendError(session, errcode::PARAM_MISSING, "Required parameter missing");
        return -1;
    }

    int64_t uid = std::stoll(uid_str);
    int64_t token_time = std::stoll(token_time_str);

    if (token_time <= time(0)) {
        INFO(logger) << "[Presence] onConnect FAIL: token expired, token_time=" << token_time << " now=" << time(0);
        sendError(session, errcode::NOT_LOGIN, "Token expired, please re-login");
        return -1;
    }

    data::UserInfo::ptr uinfo = UserMgr::GetInstance()->get(uid);
    if (!uinfo) {
        INFO(logger) << "[Presence] onConnect FAIL: user not found uid=" << uid;
        sendError(session, errcode::USER_NOT_FOUND, "User not found");
        return -1;
    }
    if (uinfo->getState() != 1) {
        INFO(logger) << "[Presence] onConnect FAIL: user state=" << uinfo->getState() << " uid=" << uid;
        sendError(session, errcode::ACCOUNT_INVALID, "Account status abnormal");
        return -1;
    }

    // 以设备表 token 为准校验（Redis 优先，DB 兜底）
    if (!UserLoginDeviceMgr::GetInstance()->validateToken(uid, token, time(0))) {
        INFO(logger) << "[Presence] onConnect FAIL: token mismatch, uid=" << uid;
        sendError(session, errcode::NOT_LOGIN, "Token mismatch, please re-login");
        return -1;
    }

    NotificationMgr::GetInstance()->addPresenceConnection(uid, session);
    INFO(logger) << "[Presence] onConnect OK: uid=" << uid;

    broadcastOnlineCount();
    return 0;
}

int32_t PresenceServlet::onClose(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session)
{
    std::string uid_str = header->getParam("uid");
    if (!uid_str.empty()) {
        int64_t uid = std::stoll(uid_str);
        NotificationMgr::GetInstance()->removePresenceConnection(uid);
        broadcastOnlineCount();
    }

    return 0;
}

int32_t PresenceServlet::handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session)
{
    // 心跳处理
    Json::Value root;
    if (chen::JsonUtil::FromString(root, msg->getData()) && root.isObject()) {
        if (root["type"].asString() == "ping") {
            session->sendMessage("{\"type\":\"pong\"}");
            return 0;
        }
    }
    return 0;
}

} // namespace blog::servlet
