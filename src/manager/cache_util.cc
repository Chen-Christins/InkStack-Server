#include "cache_util.h"

#include <chen/config/config.h>
#include <chen/db/query_builder.h>
#include <chen/db/redis.h>
#include <chen/util/util.h>

namespace blog {

static chen::ConfigVar<std::string>::ptr g_redis_pool_name = chen::Config::Lookup("redis.name", std::string("blog"), "Redis connection pool name");

int64_t executeCountCached(chen::QueryBuilder::ptr qb, chen::IDB::ptr conn, const std::string& cacheKey, int ttlSec)
{
    std::string redisKey = "cache:count:" + cacheKey;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", redisKey.c_str());
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    int64_t total = 0;
    if (qb->executeCount(total, conn)) {
        return 0;
    }
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %lld", redisKey.c_str(), ttlSec, total);
    return total;
}

void cacheListResult(const std::string& cacheKey, const std::vector<int64_t>& ids, int ttlSec)
{
    if (ids.empty()) {
        return;
    }
    std::stringstream ss;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) {
            ss << ",";
        }
        ss << ids[i];
    }
    std::string redisKey = "cache:list:" + cacheKey;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %s", redisKey.c_str(), ttlSec, ss.str().c_str());
}

bool getCachedListResult(const std::string& cacheKey, std::vector<int64_t>& ids)
{
    std::string redisKey = "cache:list:" + cacheKey;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", redisKey.c_str());
    if (!rpy || !rpy->str) {
        return false;
    }
    std::string data(rpy->str);
    size_t pos = 0;
    while (pos < data.size()) {
        size_t end = data.find(',', pos);
        if (end == std::string::npos) {
            end = data.size();
        }
        ids.push_back(std::stoll(data.substr(pos, end - pos)));
        pos = end + 1;
    }
    return true;
}

void cacheStringResult(const std::string& cacheKey, const std::string& value, int ttlSec)
{
    std::string redisKey = "cache:str:" + cacheKey;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %s", redisKey.c_str(), ttlSec, value.c_str());
}

bool getCachedStringResult(const std::string& cacheKey, std::string& value)
{
    std::string redisKey = "cache:str:" + cacheKey;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", redisKey.c_str());
    if (!rpy || !rpy->str) {
        return false;
    }
    value = rpy->str;
    return true;
}

void cacheIdMapping(const std::string& cacheKey, int64_t id, int ttlSec)
{
    std::string redisKey = "cache:map:" + cacheKey;
    chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "setex %s %d %lld", redisKey.c_str(), ttlSec, id);
}

int64_t getCachedIdMapping(const std::string& cacheKey)
{
    std::string redisKey = "cache:map:" + cacheKey;
    auto rpy = chen::RedisUtil::Cmd(g_redis_pool_name->getValue(), "get %s", redisKey.c_str());
    if (rpy && rpy->str) {
        return chen::TypeUtil::Atoi(rpy->str);
    }
    return 0;
}

} // namespace blog
