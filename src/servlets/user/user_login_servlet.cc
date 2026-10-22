#include "user_login_servlet.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>

#include "../../manager/system_settings_manager.h"
#include "../../manager/user_login_device_manager.h"
#include "../../manager/user_manager.h"
#include "../../util.h"
#include "../../util/ua_parser.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

/// Redis 2FA 临时凭证 key 前缀（5分钟有效）
static const char* k2faPendingPrefix = "2fa_pending:";
/// 2FA 临时凭证 TTL（秒）
static const int k2faPendingTTL = 300;

UserLoginServlet::UserLoginServlet()
    : BlogServlet("UserLoginServlet")
{
}

int32_t UserLoginServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_STRING(result, auth_id, "auth_id");
        DEFINE_AND_CHECK_STRING(result, passwd, "passwd");

        auto sdata = getSessionData(request, response);
        if (sdata->getData<int64_t>(CookieKey::USER_ID)) {
            result->setErrno(errcode::USER_ALREADY_LOGIN);
            break;
        }

        data::UserInfo::ptr info;
        if (IsEmail(auth_id)) {
            info = UserMgr::GetInstance()->getByEmail(auth_id);
        } else if (IsValidAccount(auth_id)) {
            info = UserMgr::GetInstance()->getByAccount(auth_id);
        } else {
            result->setErrno(errcode::USER_INVALID_ACCOUNT);
            break;
        }

        if (!info) {
            result->setErrno(errcode::AUTH_CODE_INVALID);
            break;
        }
        if (!VerifyPassword(passwd, info->getPasswd())) {
            result->setErrno(errcode::USER_PASSWORD_WRONG);
            break;
        }

        if (info->getState() != 1 || info->getIsDeleted()) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        // 历史 MD5 哈希在登录成功后透明升级为 bcrypt；失败不影响本次登录，下次登录重试
        if (IsLegacyPasswordHash(info->getPasswd())) {
            std::string passwd_hash = HashPassword(passwd);
            auto upgrade_db = getDB();
            if (!passwd_hash.empty() && upgrade_db) {
                std::string old_hash = info->getPasswd();
                info->setPasswd(passwd_hash);
                if (data::UserInfoDao::Update(info, upgrade_db) == 0) {
                    UserMgr::GetInstance()->update(info);
                } else {
                    info->setPasswd(old_hash);
                    ERROR(logger) << "upgrade password hash failed user=" << info->getId();
                }
            }
        }

        // 2FA: 密码验证通过后，如果启用了TOTP，返回临时凭证等待第二步验证
        if (info->getTotpEnabled()) {
            std::string tempToken = UserManager::generateToken();
            std::string redisKey = std::string(k2faPendingPrefix) + tempToken;
            chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %lld", redisKey.c_str(), k2faPendingTTL, (long long)info->getId());
            result->set("require_2fa", true);
            result->set("temp_token", tempToken);
            INFO(logger) << "login user=" << info->getId() << " require 2fa verification";
            break;
        }

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

        // 设备标识：优先客户端上报 X-Device-Id，否则回退/生成持久 DEVICE_ID cookie
        std::string device_id = request->getHeader("X-Device-Id");
        if (device_id.empty()) {
            device_id = request->getCookie(CookieKey::DEVICE_ID);
        }
        if (device_id.empty()) {
            device_id = UserManager::generateToken();
            // 长期持久 cookie（约 10 年），保证网页端设备标识稳定
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

        // 记录本次登录
        UserLoginDeviceMgr::GetInstance()->recordLogin(param);

        info->setLoginTime(now);
        if (data::UserInfoDao::Update(info, db) == 0) {
            UserMgr::GetInstance()->update(info);
        }
        INFO(logger) << "login user=" << info->getId() << " platform=" << platform << " device=" << device_name;

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
