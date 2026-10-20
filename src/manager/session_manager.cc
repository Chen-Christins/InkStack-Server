#include "session_manager.h"

#include <chen/config/config.h>
#include <chen/db/redis.h>
#include <chen/log/log.h>
#include <chen/util/type_util.h>

#include <memory>
#include <string>

#include "system_settings_manager.h"

namespace blog {

/// Redis session key 前缀
static const char* kSessionKeyPrefix = "session:";
/// 默认 session TTL（24 小时 = 86400 秒）
static const int64_t kDefaultSessionTTL = 24 * 3600;

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

int64_t GetSessionTTL()
{
    auto settings = SystemSettingsMgr::GetInstance()->get();
    if (settings && settings->getSessionTimeout() > 0) {
        return static_cast<int64_t>(settings->getSessionTimeout()) * 3600;
    }
    return kDefaultSessionTTL;
}

void SaveSessionToRedis(const std::string& session_id, int64_t uid, int32_t is_auth)
{
    std::string key = std::string(kSessionKeyPrefix) + session_id;
    int64_t ttl = GetSessionTTL();
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hset %s user_id %lld", key.c_str(), (long long)uid);
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hset %s is_auth %d", key.c_str(), is_auth);
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "expire %s %lld", key.c_str(), (long long)ttl);
}

chen::http::SessionData::ptr LoadSessionFromRedis(const std::string& session_id)
{
    std::string key = std::string(kSessionKeyPrefix) + session_id;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "hgetall %s", key.c_str());
    if (!rpy || rpy->type != REDIS_REPLY_ARRAY || rpy->elements == 0) {
        return nullptr;
    }

    int64_t uid = 0;
    int32_t is_auth = 0;

    for (size_t i = 0; i + 1 < rpy->elements; i += 2) {
        if (!rpy->element[i] || !rpy->element[i]->str || !rpy->element[i + 1] || !rpy->element[i + 1]->str) {
            continue;
        }
        std::string field(rpy->element[i]->str, rpy->element[i]->len);
        std::string value(rpy->element[i + 1]->str, rpy->element[i + 1]->len);
        if (field == "user_id") {
            uid = chen::TypeUtil::Atoi(value);
        } else if (field == "is_auth") {
            is_auth = (int32_t)chen::TypeUtil::Atoi(value);
        }
    }

    if (!uid) {
        return nullptr;
    }

    auto data = std::make_shared<chen::http::SessionData>(false);
    if (!data) {
        return nullptr;
    }
    data->setId(session_id);
    data->setData("S_UID", uid);
    data->setData("IS_AUTH", is_auth);

    // 注册回内存 SessionDataMgr
    chen::http::SessionDataMgr::GetInstance()->add(data);

    // 刷新 Redis TTL
    int64_t ttl = GetSessionTTL();
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "expire %s %lld", key.c_str(), (long long)ttl);

    return data;
}

void DeleteSessionFromRedis(const std::string& session_id)
{
    std::string key = std::string(kSessionKeyPrefix) + session_id;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "del %s", key.c_str());
}

} // namespace blog
