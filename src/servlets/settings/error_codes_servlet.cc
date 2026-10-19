#include "error_codes_servlet.h"

#include "../../i18n/i18n.h"
#include "../../manager/error_code_manager.h"

namespace blog::servlet {

ErrorCodesServlet::ErrorCodesServlet()
    : BlogServlet("ErrorCodesServlet")
{
}

int32_t ErrorCodesServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    response->setHeader("Vary", "Accept-Language");

    do {
        std::string lang = I18n::ResolveLanguage(request);

        auto manager = ErrorCodeMgr::GetInstance();
        if (manager->getCount() == 0) {
            result->setErrno(errcode::SETTINGS_NOT_LOADED);
            break;
        }

        result->set("version", manager->getVersion());
        result->setDataJson(manager->toJson(lang));
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace blog::servlet
