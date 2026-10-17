#include "util.h"

#include <chen/config/config.h>

namespace blog {

static chen::ConfigVar<std::map<std::string, std::map<std::string, std::string>>>::ptr g_mysql_dbs =
    chen::Config::Lookup("mysql.dbs", std::map<std::string, std::map<std::string, std::string>>(), "mysql dbs");

chen::IDB::ptr GetDB()
{
    return chen::MySQLMgr::GetInstance()->get(g_mysql_dbs->getValue().begin()->first);
}

} // namespace blog