#include "util.h"

#include <chen/config/config.h>
#include <chen/log/log.h>

namespace blog {

static chen::Logger::ptr g_logger = LOG_ROOT();

static chen::ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_mysql_dbs =
    chen::Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>>(), "mysql dbs");

chen::IDB::ptr GetDB()
{
    const auto& dbs = g_mysql_dbs->getValue();
    if (dbs.empty()) {
        ERROR(g_logger) << "mysql.dbs is empty";
        return nullptr;
    }
    return chen::MySQLMgr::GetInstance()->get(dbs.begin()->first);
}

std::string HashPassword(const std::string& password)
{
    std::string hash = chen::EncryptorUtil::BCryptHash(password);
    if (hash.empty()) {
        ERROR(g_logger) << "bcrypt hash password failed";
    }
    return hash;
}

bool VerifyPassword(const std::string& password, const std::string& stored)
{
    if (stored.empty()) {
        return false;
    }
    if (stored.size() > 1 && stored[0] == '$' && stored[1] == '2') {
        return chen::EncryptorUtil::BCryptVerify(password, stored);
    }
    // 兼容历史无盐 MD5，登录成功后由调用方升级为 bcrypt
    return stored == chen::EncryptorUtil::MD5(password);
}

bool IsLegacyPasswordHash(const std::string& stored)
{
    if (stored.size() != 32) {
        return false;
    }
    for (char c : stored) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

} // namespace blog
