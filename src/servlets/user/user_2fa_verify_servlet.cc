#include "user_2fa_verify_servlet.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>

#include "../../manager/system_settings_manager.h"
#include "../../manager/user_login_device_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"
#include "../../util/totp_util.h"
#include "../../util/ua_parser.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

/// Redis 2FA 临时凭证 key 前缀（5分钟有效）
static const char* k2faPendingPrefix = "2fa_pending:";

User2faVerifyServlet::User2faVerifyServlet()
    : BlogServlet("User2faVerifyServlet")
{
}

int32_t User2faVerifyServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, tempToken, "temp_token");
        DEFINE_AND_CHECK_STRING(result, code, "code");

        // 从Redis获取临时凭证对应的用户ID
        std::string key = std::string(k2faPendingPrefix) + tempToken;
        auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", key.c_str());
        int64_t uid = 0;
        if (rpy && rpy->type == REDIS_REPLY_STRING && rpy->str) {
            try {
                uid = std::stoll(std::string(rpy->str, rpy->len));
            } catch (...) {
                uid = 0;
            }
        }
        if (!uid) {
            result->setErrno(errcode::TEMP_TOKEN_INVALID);
            break;
        }

        // 删除临时凭证（一次性）
        chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", key.c_str());

        auto info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }

        if (info->getState() != 1 || info->getIsDeleted()) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        // 验证TOTP码
        if (!TotpUtil::VerifyCode(info->getTotpSecret(), code, 1)) {
            result->setErrno(errcode::TOTP_INVALID_CODE);
            break;
        }

        // TOTP验证通过，完成登录（复用login servlet的逻辑）
        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }

        int64_t now = time(0);
        int32_t session_timeout = 24;
        auto sys_settings = SystemSettingsMgr::GetInstance()->get();
        if (sys_settings && sys_settings->getSessionTimeout() > 0) {
            session_timeout = sys_settings->getSessionTimeout();
        }
        int64_t token_time = now + 3600 * session_timeout;
        std::string token = UserManager::generateToken();

        // 设备信息
        std::string ua = request->getHeader("User-Agent");
        const char* platform = DetectPlatform(ua);
        std::string device_name = ParseDeviceName(ua);
        std::string ip = GetRemoteIP(request, session);

        std::string device_id = request->getHeader("X-Device-Id");
        if (device_id.empty()) {
            device_id = request->getCookie(CookieKey::DEVICE_ID);
        }
        if (device_id.empty()) {
            device_id = UserManager::generateToken();
            response->setCookie(CookieKey::DEVICE_ID, device_id, time(0) + 10 * 365 * 24 * 3600, "/");
        }

        // 设备唯一：同一设备被其他账号占用时顶掉
        auto device_owner = UserLoginDeviceMgr::GetInstance()->getActiveByDevice(device_id);
        if (device_owner && device_owner->getUserId() != info->getId()) {
            UserLoginDeviceMgr::GetInstance()->kick(device_owner);
        }

        // 同平台互顶：顶掉该用户同平台旧设备
        auto same_platform = UserLoginDeviceMgr::GetInstance()->getActiveByUserAndPlatform(info->getId(), platform);
        if (same_platform) {
            UserLoginDeviceMgr::GetInstance()->kick(same_platform);
        }

        LoginParam param = {};
        param.uid = info->getId();
        param.device_id = device_id;
        param.platform = platform;
        param.device_name = device_name;
        param.user_agent = ua;
        param.ip = ip;
        param.token = token;
        param.token_time = token_time;

        UserLoginDeviceMgr::GetInstance()->recordLogin(param);

        info->setLoginTime(now);
        data::UserInfoDao::Update(info, db);
        INFO(logger) << "2fa login user=" << info->getId() << " platform=" << platform << " device=" << device_name;

        auto sdata = getSessionData(request, response);
        response->setCookie(CookieKey::USER_ID, EncryptUserId(info->getId()), token_time, "/");
        response->setCookie(CookieKey::TOKEN, token, token_time, "/");
        response->setCookie(CookieKey::TOKEN_TIME, std::to_string(token_time), token_time, "/");
        sdata->setData(CookieKey::USER_ID, info->getId());
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
