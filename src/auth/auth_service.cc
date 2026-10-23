#include "auth_service.h"

#include <ctime>

#include <chen/config/config.h>
#include <chen/http/session_data.h>
#include <chen/log/log.h>
#include <chen/util/type_util.h>

#include "../error_codes.h"
#include "../manager/session_manager.h"
#include "../manager/user_login_device_manager.h"
#include "../manager/user_manager.h"
#include "../struct.h"
#include "../util.h"
#include "session_store.h"

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<bool>::ptr g_cookie_secure = chen::Config::Lookup("auth.cookie_secure", false, "Set Secure flag on session cookie");
static chen::ConfigVar<std::string>::ptr g_cookie_same_site =
    chen::Config::Lookup("auth.cookie_same_site", std::string("Lax"), "Session cookie SameSite: Lax/Strict/None");
static chen::ConfigVar<int64_t>::ptr g_session_absolute_ttl =
    chen::Config::Lookup("auth.session_absolute_ttl", (int64_t)(7 * 24 * 3600), "Absolute session lifetime in seconds, <=0 disables it");
static chen::ConfigVar<bool>::ptr g_ws_legacy_token =
    chen::Config::Lookup("auth.ws_legacy_token", true, "Allow legacy token query auth for WebSocket during migration");

static std::string NormalizeSameSite()
{
    std::string value = g_cookie_same_site->getValue();
    if (value != "Lax" && value != "Strict" && value != "None") {
        return "Lax";
    }
    return value;
}

static bool UseSecureCookie()
{
    return g_cookie_secure->getValue() || NormalizeSameSite() == "None";
}

static void ClearLegacyCookies(chen::http::HttpResponse::ptr response)
{
    if (!response) {
        return;
    }
    int64_t past = time(0) - 3600 * 24;
    response->setCookie(CookieKey::USER_ID, "", past, "/");
    response->setCookie(CookieKey::TOKEN, "", past, "/");
    response->setCookie(CookieKey::TOKEN_TIME, "", past, "/");
}

void AuthService::MirrorSessionData(const std::string& session_id, int64_t uid)
{
    if (session_id.empty() || !uid) {
        return;
    }

    auto manager = chen::http::SessionDataMgr::GetInstance();
    auto data = manager->get(session_id);
    if (!data) {
        data = std::make_shared<chen::http::SessionData>(false);
        data->setId(session_id);
    }
    data->setData(CookieKey::USER_ID, uid);
    data->setData(CookieKey::IS_AUTH, 1);
    manager->add(data);
}

AuthContext AuthService::Resolve(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, bool touch)
{
    AuthContext ctx;
    if (!request) {
        return ctx;
    }

    std::string session_id = request->getCookie(CookieKey::SESSION_KEY);
    if (session_id.empty()) {
        return ctx;
    }

    // 匿名会话不参与认证，也不要清除 Cookie（公共接口会用到）
    auto sdata = chen::http::SessionDataMgr::GetInstance()->get(session_id);
    if (sdata && sdata->getData<int64_t>(CookieKey::USER_ID) == 0) {
        return ctx;
    }

    SessionInfo info;
    if (!SessionStore::Get(session_id, info)) {
        chen::http::SessionDataMgr::GetInstance()->del(session_id);
        DeleteSessionFromRedis(session_id);
        if (response) {
            ClearSessionCookie(response);
            ClearLegacyCookies(response);
        }
        return ctx;
    }

    auto user = UserMgr::GetInstance()->get(info.uid);
    if (!user || user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
        SessionStore::Destroy(session_id);
        chen::http::SessionDataMgr::GetInstance()->del(session_id);
        DeleteSessionFromRedis(session_id);
        if (response) {
            ClearSessionCookie(response);
            ClearLegacyCookies(response);
        }
        return ctx;
    }

    int64_t now = time(0);
    if (touch && now - info.last_seen >= 60) {
        if (!SessionStore::Touch(session_id, info, GetSessionTTL(), g_session_absolute_ttl->getValue())) {
            chen::http::SessionDataMgr::GetInstance()->del(session_id);
            DeleteSessionFromRedis(session_id);
            if (response) {
                ClearSessionCookie(response);
                ClearLegacyCookies(response);
            }
            return ctx;
        }
    }

    MirrorSessionData(session_id, info.uid);
    ctx.uid = info.uid;
    ctx.session_id = session_id;
    ctx.user = user;
    return ctx;
}

bool AuthService::UpgradeLegacySession(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session)
{
    if (!request || !response) {
        return false;
    }

    int64_t uid = DecryptUserId(request->getCookie(CookieKey::USER_ID));
    if (!uid) {
        return false;
    }

    std::string token = request->getCookie(CookieKey::TOKEN);
    if (token.empty()) {
        return false;
    }

    int64_t token_time = request->getCookieAs<int64_t>(CookieKey::TOKEN_TIME);
    if (token_time <= time(0)) {
        return false;
    }

    auto user = UserMgr::GetInstance()->get(uid);
    if (!user) {
        return false;
    }
    if (user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
        return false;
    }
    if (!UserLoginDeviceMgr::GetInstance()->validateToken(uid, token, time(0))) {
        INFO(logger) << "legacy token invalid uid=" << uid << " path=" << request->getPath();
        return false;
    }

    IssueOptions options;
    options.update_login_time = false;
    if (IssueSession(request, response, session, user, options) != 0) {
        return false;
    }

    INFO(logger) << "legacy session upgraded uid=" << uid << " path=" << request->getPath();
    return true;
}

int32_t AuthService::IssueSession(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    data::UserInfo::ptr user,
    const IssueOptions& options)
{
    if (!request || !response || !user) {
        return errcode::INTERNAL_ERROR;
    }
    if (user->getState() != UserManager::Status::ACTIVE || user->getIsDeleted()) {
        return errcode::ACCOUNT_INVALID;
    }

    int64_t idle_ttl = GetSessionTTL();
    int64_t absolute_ttl = g_session_absolute_ttl->getValue();

    std::string ip;
    if (session) {
        ip = GetRemoteIP(request, session);
    } else {
        ip = request->getHeader("X-Real-IP");
    }
    std::string user_agent = request->getHeader("User-Agent");
    std::string old_session_id = request->getCookie(CookieKey::SESSION_KEY);

    std::string session_id = SessionStore::Create(user->getId(), ip, user_agent, idle_ttl, absolute_ttl);
    if (session_id.empty()) {
        return errcode::REDIS_OPERATION_FAILED;
    }

    if (!old_session_id.empty() && old_session_id != session_id) {
        SessionStore::Destroy(old_session_id);
        chen::http::SessionDataMgr::GetInstance()->del(old_session_id);
        DeleteSessionFromRedis(old_session_id);
    }

    MirrorSessionData(session_id, user->getId());
    int64_t cookie_max_age = absolute_ttl > 0 ? absolute_ttl : 0;
    SetSessionCookie(response, session_id, cookie_max_age);
    if (!request->getCookie(CookieKey::USER_ID).empty() || !request->getCookie(CookieKey::TOKEN).empty()) {
        ClearLegacyCookies(response);
    }
    request->setCookie(CookieKey::SESSION_KEY, session_id);

    if (options.update_login_time) {
        user->setLoginTime(time(0));
        auto db = GetDB();
        if (db && data::UserInfoDao::Update(user, db) == 0) {
            UserMgr::GetInstance()->update(user);
        } else {
            WARN(logger) << "update login time failed user=" << user->getId();
        }
    }

    INFO(logger) << "session issued user=" << user->getId() << " sid=" << SessionStore::HashId(session_id).substr(0, 12) << " ip=" << ip;
    return 0;
}

void AuthService::RevokeSession(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, const AuthContext& ctx)
{
    std::string session_id = ctx.session_id;
    if (session_id.empty() && request) {
        session_id = request->getCookie(CookieKey::SESSION_KEY);
    }

    if (!session_id.empty()) {
        SessionStore::Destroy(session_id);
        chen::http::SessionDataMgr::GetInstance()->del(session_id);
        DeleteSessionFromRedis(session_id);
    }

    if (request) {
        std::string token = request->getCookie(CookieKey::TOKEN);
        if (!token.empty()) {
            UserLoginDeviceMgr::GetInstance()->logout(token);
        }
    }

    ClearSessionCookie(response);
    ClearLegacyCookies(response);
}

void AuthService::RevokeAllSessions(int64_t uid)
{
    if (!uid) {
        return;
    }

    SessionStore::DestroyAll(uid);
    UserLoginDeviceMgr::GetInstance()->revokeAllByUser(uid);
    INFO(logger) << "revoked all sessions uid=" << uid;
}

bool AuthService::ResolveWs(chen::http::HttpRequest::ptr request, int64_t& uid, data::UserInfo::ptr& user)
{
    if (!request) {
        return false;
    }

    AuthContext ctx = Resolve(request, nullptr, true);
    if (ctx.valid()) {
        uid = ctx.uid;
        user = ctx.user;
        return true;
    }

    if (!g_ws_legacy_token->getValue()) {
        return false;
    }

    std::string uid_str = request->getParam("uid");
    std::string token = request->getParam("token");
    std::string token_time_str = request->getParam("token_time");
    if (uid_str.empty() || token.empty() || token_time_str.empty()) {
        return false;
    }

    int64_t legacy_uid = 0;
    int64_t token_time = 0;
    try {
        legacy_uid = chen::TypeUtil::Atoi(uid_str);
        token_time = chen::TypeUtil::Atoi(token_time_str);
    } catch (...) {
        return false;
    }
    if (!legacy_uid || token_time <= time(0)) {
        return false;
    }

    auto legacy_user = UserMgr::GetInstance()->get(legacy_uid);
    if (!legacy_user || legacy_user->getState() != UserManager::Status::ACTIVE || legacy_user->getIsDeleted()) {
        return false;
    }
    if (!UserLoginDeviceMgr::GetInstance()->validateToken(legacy_uid, token, time(0))) {
        return false;
    }

    uid = legacy_uid;
    user = legacy_user;
    WARN(logger) << "ws legacy token auth uid=" << legacy_uid;
    return true;
}

void AuthService::SetSessionCookie(chen::http::HttpResponse::ptr response, const std::string& session_id, int64_t max_age)
{
    if (!response) {
        return;
    }

    std::string cookie = CookieKey::SESSION_KEY + "=" + session_id + "; Path=/; HttpOnly; SameSite=" + NormalizeSameSite();
    if (UseSecureCookie()) {
        cookie += "; Secure";
    }
    if (max_age > 0) {
        cookie += "; Max-Age=" + std::to_string(max_age);
    }
    response->setHeader("Set-Cookie", cookie);
}

void AuthService::ClearSessionCookie(chen::http::HttpResponse::ptr response)
{
    if (!response) {
        return;
    }

    std::string cookie = CookieKey::SESSION_KEY + "=; Path=/; HttpOnly; SameSite=" + NormalizeSameSite() + "; Max-Age=0";
    if (UseSecureCookie()) {
        cookie += "; Secure";
    }
    response->setHeader("Set-Cookie", cookie);
}

} // namespace blog
