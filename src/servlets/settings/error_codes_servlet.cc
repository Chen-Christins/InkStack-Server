#include "error_codes_servlet.h"

#include "../../manager/error_code_manager.h"

#include <chen/log/log.h>

namespace blog::servlet {

static chen::Logger::ptr logger = LOG_ROOT();

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
    do {
        // 解析语言参数: ?lang=zh-CN 或 ?lang=en-US
        std::string lang = request->getParam("lang");
        if (lang.empty()) {
            // 尝试从 Accept-Language 请求头解析
            std::string acceptLang = request->getHeader("Accept-Language");
            if (!acceptLang.empty()) {
                size_t comma = acceptLang.find(',');
                std::string first = (comma != std::string::npos) ? acceptLang.substr(0, comma) : acceptLang;
                size_t dash = first.find('-');
                lang = (dash != std::string::npos) ? first.substr(0, dash) : first;
                if (lang != "zh" && lang != "en") {
                    lang = "zh";
                }
            } else {
                lang = "zh";
            }
        } else {
            if (lang.find("en") != std::string::npos) {
                lang = "en";
            } else {
                lang = "zh";
            }
        }

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
