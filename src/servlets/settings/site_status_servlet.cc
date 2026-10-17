#include "site_status_servlet.h"

#include "../../manager/system_settings_manager.h"

#include <chen/log/log.h>

namespace blog::servlet {

static chen::Logger::ptr logger = LOG_ROOT();

SiteStatusServlet::SiteStatusServlet()
    : BlogServlet("SiteStatusServlet")
{
}

int32_t SiteStatusServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        auto settings = SystemSettingsMgr::GetInstance()->get();
        if (!settings) {
            result->setErrno(errcode::SETTINGS_NOT_LOADED);
            break;
        }
        result->set("maintenanceMode", settings->getMaintenanceMode() ? true : false);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace blog::servlet
