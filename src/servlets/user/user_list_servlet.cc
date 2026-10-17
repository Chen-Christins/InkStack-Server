#include "user_list_servlet.h"

#include <chen/log/log.h>

#include <algorithm>

#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserListServlet::UserListServlet()
    : BlogLoginedServlet("UserListServlet")
{
}

int32_t UserListServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }
        auto current_user = UserMgr::GetInstance()->get(uid);
        if (!current_user) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        int32_t role = current_user->getRole();

        if (role != UserManager::Role::ADMIN) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        std::vector<int64_t> ids;
        UserMgr::GetInstance()->getAllIds(ids, false);

        std::sort(ids.begin(), ids.end(), std::less<>());

        for (size_t i = 0; i < ids.size(); ++i) {
            result->jsondata["ids"].append(ids[i]);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
