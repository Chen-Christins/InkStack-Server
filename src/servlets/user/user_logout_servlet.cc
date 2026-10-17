#include "user_logout_servlet.h"

#include <chen/log/log.h>

#include "../../include/tables.h"
#include "../../manager/session_manager.h"
#include "../../manager/user_login_device_manager.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserLogoutServlet::UserLogoutServlet()
    : BlogServlet("UserLogoutServlet")
{
}

int32_t UserLogoutServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        auto sdata = getSessionData(request, response);
        if (!sdata->getData<int64_t>(CookieKey::USER_ID)) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        result->setErrno(errcode::SUCCESS);

        // 只退出当前设备（另一平台/设备不受影响）
        std::string token = request->getCookie(CookieKey::TOKEN);
        if (!token.empty()) {
            UserLoginDeviceMgr::GetInstance()->logout(token);
        }

        int64_t token_time = time(0) - 3600 * 24;
        response->setCookie(CookieKey::USER_ID, "", token_time, "/");
        response->setCookie(CookieKey::TOKEN, "", token_time, "/");
        response->setCookie(CookieKey::TOKEN_TIME, "", token_time, "/");
        sdata->setData(CookieKey::USER_ID, (int64_t)0);
        std::string id = sdata->getId();
        chen::http::SessionDataMgr::GetInstance()->del(id);
        DeleteSessionFromRedis(id);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
