#include "struct.h"

#include <chen/log/log.h>

#include "blog/data/user_info.h"
#include "error_codes.h"
#include "manager/error_code_manager.h"
#include "manager/session_manager.h"
#include "manager/user_login_device_manager.h"
#include "manager/user_manager.h"
#include "util.h"

namespace blog {

static chen::Logger::ptr logger = LOG_NAME("access");

const std::string CookieKey::SESSION_KEY = "SESSIONID";
const std::string CookieKey::USER_ID = "S_UID";
const std::string CookieKey::TOKEN = "S_TOKEN";
const std::string CookieKey::TOKEN_TIME = "S_TOKEN_TIME";
const std::string CookieKey::IS_AUTH = "IS_AUTH";
const std::string CookieKey::DEVICE_ID = "DEVICE_ID";

std::string GetRemoteIP(chen::http::HttpRequest::ptr request, chen::http::HttpSession::ptr session)
{
    auto rt = request->getHeader("X-Real-IP");
    if (!rt.empty()) {
        return rt;
    }
    rt = session->getRemoteAddressString();
    auto pos = rt.find(':');
    return rt.substr(0, pos);
}

Result::Result(int32_t ec, const std::string& m)
    : errno_(ec)
    , used(chen::TimeUtil::GetCurrentUs())
    , msg(m)
{
}

void Result::setErrno(int32_t ec)
{
    errno_ = ec;
    msg = ErrorCodeMgr::GetInstance()->getMessage(ec);
}

void Result::setErrno(int32_t ec, const std::string& customMsg)
{
    errno_ = ec;
    msg = customMsg;
}

void Result::setDataJson(const std::string& jsonStr)
{
    Json::Value v;
    if (chen::JsonUtil::FromString(v, jsonStr)) {
        jsondata = v;
    }
}

std::string Result::toJsonString() const
{
    Json::Value v;
    v["errno"] = errno_;
    v["msg"] = msg;
    v["used"] = ((chen::TimeUtil::GetCurrentUs() - used) / 1000.0);
    if (!jsondata.isNull()) {
        v["data"] = jsondata;
    }
    return chen::JsonUtil::ToString(v);
}

BlogServlet::BlogServlet(const std::string& name)
    : chen::http::Servlet(name)
{
}

int32_t BlogServlet::handle(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, chen::http::HttpSession::ptr session)
{
    uint64_t ts = chen::TimeUtil::GetCurrentUs();
    Result::ptr result = std::make_shared<Result>();
    // response->setHeader("Access-Control-Allow-Origin", "*");
    // response->setHeader("Access-Control-Allow-Credentials", "true");

    TRACE(logger) << "path=" << request->getPath() << " - method=" << chen::http::HttpMethodToString(request->getMethod());

    if (handlePre(request, response, session, result)) {
        handle(request, response, session, result);
    } else {
        response->setBody(result->toJsonString());
    }
    uint64_t used = chen::TimeUtil::GetCurrentUs() - ts;
    handlePost(request, response, session, result);
    response->setHeader("used", std::to_string((used * 1.0 / 1000)) + "ms");
    return 0;
}

bool BlogServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (request->getPath() != "/user/login" && request->getPath() != "/user/logout") {
        initLogin(request, response, session);
    }
    if (request->getMethod() != chen::http::HttpMethod::GET && request->getMethod() != chen::http::HttpMethod::POST) {
        result->setErrno(errcode::INVALID_METHOD);
        return false;
    }
    return true;
}

bool BlogServlet::handlePost(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    response->setHeader("Content-Type", "application/json");
    INFO(logger) << GetRemoteIP(request, session) << "\t" << request->getCookie(CookieKey::SESSION_KEY, "-") << "\t" << getUserId(request) << "\t"
                 << result->errno_ << "\t" << result->msg << "\t" << request->getPath() << "\t"
                 << (!request->getQuery().empty() ? request->getQuery() : "-");
    return true;
}

chen::http::SessionData::ptr BlogServlet::getSessionData(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response)
{
    std::string sid = request->getCookie(CookieKey::SESSION_KEY);
    if (!sid.empty()) {
        auto data = chen::http::SessionDataMgr::GetInstance()->get(sid);
        if (data) {
            return data;
        }
        // 内存未命中，尝试从 Redis 恢复（进程重启后内存清空，但 Redis 中仍有会话）
        data = LoadSessionFromRedis(sid);
        if (data) {
            return data;
        }
    }
    // 没有就创建一个会话
    chen::http::SessionData::ptr data(new chen::http::SessionData(true));
    chen::http::SessionDataMgr::GetInstance()->add(data);
    response->setCookie(CookieKey::SESSION_KEY, data->getId(), 0, "/");
    request->setCookie(CookieKey::SESSION_KEY, data->getId());
    return data;
}

bool BlogServlet::initLogin(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, chen::http::HttpSession::ptr session)
{
    auto data = getSessionData(request, response);
    int64_t uid = data->getData<int64_t>(CookieKey::USER_ID);
    if (uid) {
        // 即使 session 已登录，也要校验 token 是否仍然有效（同平台互顶 / 设备唯一）
        data::UserInfo::ptr uinfo = UserMgr::GetInstance()->get(uid);
        if (uinfo && uinfo->getState() == 1) {
            auto cookie_token = request->getCookie(CookieKey::TOKEN);
            if (!UserLoginDeviceMgr::GetInstance()->validateToken(uid, cookie_token, time(0))) {
                // token 不匹配，已被其他设备/平台登录顶掉
                data->setData(CookieKey::USER_ID, (int64_t)0);
                data->setData(CookieKey::IS_AUTH, (int32_t)1);
                return false;
            }
        }
        // 刷新 Redis 会话持久化
        SaveSessionToRedis(data->getId(), uid, 1);
        return true;
    }
    int32_t is_auth = data->getData<int32_t>(CookieKey::IS_AUTH);
    if (is_auth) {
        return false;
    }
    bool is_login = false;
    do {
        int64_t uid = DecryptUserId(request->getCookie(CookieKey::USER_ID));
        if (!uid) {
            break;
        }
        auto token = request->getCookie(CookieKey::TOKEN);
        if (token.empty()) {
            break;
        }
        int64_t token_time = request->getCookieAs<int64_t>(CookieKey::TOKEN_TIME);
        if (token_time <= time(0)) {
            break;
        }
        data::UserInfo::ptr uinfo = UserMgr::GetInstance()->get(uid);
        if (!uinfo) {
            break;
        }
        if (uinfo->getState() != 1) {
            break;
        }
        // 验证 token：以设备表 token 为准（Redis 优先，DB 兜底）
        if (!UserLoginDeviceMgr::GetInstance()->validateToken(uid, token, time(0))) {
            INFO(logger) << GetRemoteIP(request, session) << "\t" << request->getCookie(CookieKey::SESSION_KEY, "-") << "\t" << uid << "\t" << 310
                         << "\t"
                         << "invalid_token" << "\tauto_login" << request->getPath() << "\t"
                         << (!request->getQuery().empty() ? request->getQuery() : "-");
            break;
        }
        data->setData(CookieKey::USER_ID, uid);
        SaveSessionToRedis(data->getId(), uid, 1);
        is_login = true;
        INFO(logger) << GetRemoteIP(request, session) << "\t" << request->getCookie(CookieKey::SESSION_KEY, "-") << "\t" << uid << "\t" << 200 << "\t"
                     << "ok" << "\tauto_login " << request->getPath() << "\t" << (!request->getQuery().empty() ? request->getQuery() : "-");

        uinfo->setLoginTime(time(0));
        is_login = true;
    } while (0);
    data->setData(CookieKey::IS_AUTH, (int32_t)1);
    return is_login;
}

chen::IDB::ptr BlogServlet::getDB()
{
    return GetDB();
}

BlogLoginedServlet::BlogLoginedServlet(const std::string& name)
    : BlogServlet(name)
{
}

bool BlogLoginedServlet::handlePre(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    if (!initLogin(request, response, session)) {
        result->setErrno(errcode::NOT_LOGIN);
        return false;
    }
    return true;
}

int64_t BlogServlet::getUserId(chen::http::HttpRequest::ptr request)
{
    std::string sid = request->getCookie(CookieKey::SESSION_KEY);
    if (!sid.empty()) {
        auto data = chen::http::SessionDataMgr::GetInstance()->get(sid);
        if (!data) {
            // 内存未命中，尝试从 Redis 恢复
            data = LoadSessionFromRedis(sid);
        }
        if (data) {
            return data->getData<int64_t>(CookieKey::USER_ID);
        }
    }
    return 0;
}

}