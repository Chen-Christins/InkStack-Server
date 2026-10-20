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

} // namespace blog