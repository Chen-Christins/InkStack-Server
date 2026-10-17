#include "user_manager.h"

#include "../util.h"
#include "cache_util.h"

#include <chen/config/config.h>
#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr logger = LOG_ROOT();

static const size_t kCacheMaxSize = 1000;

static chen::ConfigVar<std::string>::ptr g_admin_account = chen::Config::Lookup("admin.account", std::string("admin"), "default super admin account");
static chen::ConfigVar<std::string>::ptr g_admin_passwd =
    chen::Config::Lookup("admin.passwd", std::string("admin123456"), "default super admin password");
static chen::ConfigVar<std::string>::ptr g_admin_email =
    chen::Config::Lookup("admin.email", std::string("admin@example.com"), "default super admin email");

UserManager::UserManager()
    : m_cache(32, kCacheMaxSize, 0)
{
}

data::UserInfo::ptr UserManager::parseRow(chen::ISQLData::ptr rt)
{
    return data::UserInfoDao::ParseRow(rt);
}

void UserManager::add(blog::data::UserInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

void UserManager::update(blog::data::UserInfo::ptr info)
{
    m_cache.set(info->getId(), info);
}

void UserManager::getAllIds(std::vector<int64_t>& ids, bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }
    auto qb = data::UserInfoDao::newQuery();
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->select("id");
    qb->queryColumn<int64_t>(ids, db, "id");
}

uint64_t UserManager::listByPages(
    std::vector<blog::data::UserInfo::ptr>& infos,
    uint64_t offset,
    uint64_t size,
    int32_t role,
    int32_t state,
    int32_t days,
    bool isValid)
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return 0;
    }

    int64_t start_time = 0;
    if (days > 0) {
        start_time = time(0) - days * 24 * 3600;
    }

    auto qb = data::UserInfoDao::newQuery();
    qb->whereIf(role != -1, "role", "=", (int64_t)role);
    qb->whereIf(state != -1, "state", "=", (int64_t)state);
    qb->whereIf(days > 0, "create_time", ">=", start_time);
    qb->whereIf(isValid, "is_deleted", "=", (int64_t)0);
    qb->orderBy("id", "DESC");

    int64_t total = 0;
    if (data::UserInfoDao::QueryByBuilderPages(infos, total, qb, (int32_t)offset, (int32_t)size, db)) {
        return 0;
    }
    for (auto& info : infos) {
        if (info && !m_cache.exists(info->getId())) {
            m_cache.set(info->getId(), info);
        }
    }
    return total;
}

blog::data::UserInfo::ptr UserManager::get(int64_t id)
{
    auto v = m_cache.get(id);
    if (v) {
        return v;
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    v = data::UserInfoDao::Query(id, db);
    if (v) {
        m_cache.set(id, v);
    }
    return v;
}

blog::data::UserInfo::ptr UserManager::getByAccount(const std::string& v)
{
    int64_t cachedId = getCachedIdMapping("usr:acct:" + v);
    if (cachedId > 0) {
        return get(cachedId);
    }
    // Check cache first
    // Since cache is only keyed by id, query DB directly via DAO
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::UserInfoDao::QueryByAccount(v, db);
    if (info) {
        m_cache.set(info->getId(), info);
        cacheIdMapping("usr:acct:" + v, info->getId());
    }
    return info;
}

blog::data::UserInfo::ptr UserManager::getByEmail(const std::string& v)
{
    int64_t cachedId = getCachedIdMapping("usr:eml:" + v);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::UserInfoDao::QueryByEmail(v, db);
    if (info) {
        m_cache.set(info->getId(), info);
        cacheIdMapping("usr:eml:" + v, info->getId());
    }
    return info;
}

void UserManager::ensureSuperAdmin()
{
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return;
    }

    std::string account = g_admin_account->getValue();
    std::string passwd = g_admin_passwd->getValue();
    std::string email = g_admin_email->getValue();
    if (account.empty() || passwd.empty() || email.empty()) {
        ERROR(logger) << "admin account or password empty, skip seeding super admin";
        return;
    }

    auto info = data::UserInfoDao::QueryByEmail(email, db);
    if (!info) {
        info.reset(new data::UserInfo);
        info->setPasswd(chen::EncryptorUtil::MD5(passwd));
        info->setName(account);
        info->setAccount(account);
        info->setEmail(email);
    }

    info->setRole(Role::ADMIN);
    info->setState(Status::ACTIVE);
    info->setIsDeleted(0);

    if (data::UserInfoDao::InsertOrUpdate(info, db)) {
        ERROR(logger) << "insert super admin failed: errno=" << db->getErrno() << " errstr=" << db->getErrStr();
        return;
    }
    m_cache.set(info->getId(), info);

    INFO(logger) << "seeded super admin account=" << account << " email=" << email;
}

blog::data::UserInfo::ptr UserManager::getByName(const std::string& v)
{
    int64_t cachedId = getCachedIdMapping("usr:name:" + v);
    if (cachedId > 0) {
        return get(cachedId);
    }
    auto db = GetDB();
    if (!db) {
        ERROR(logger) << "Get DB connection fail";
        return nullptr;
    }
    auto info = data::UserInfoDao::QueryByName(v, db);
    if (info) {
        m_cache.set(info->getId(), info);
        cacheIdMapping("usr:name:" + v, info->getId());
    }
    return info;
}

std::string UserManager::GetToken(data::UserInfo::ptr info, int64_t us)
{
    std::stringstream ss;
    ss << info->getId() << "|" << info->getAccount() << "|" << info->getEmail() << "|" << info->getPasswd() << "|" << us;
    return chen::EncryptorUtil::MD5(ss.str());
}

std::string UserManager::generateToken()
{
    std::stringstream ss;
    ss << std::hex << chen::TimeUtil::GetCurrentUs() << rand() << rand();
    return chen::EncryptorUtil::MD5(ss.str());
}

} // namespace blog
