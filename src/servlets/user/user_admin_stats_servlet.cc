#include "user_admin_stats_servlet.h"

#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

UserAdminStatsServlet::UserAdminStatsServlet()
    : BlogLoginedServlet("UserAdminStatsServlet")
{
}

int32_t UserAdminStatsServlet::handle(
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

        int64_t total_users = 0;
        int64_t active_users = 0;
        int64_t inactive_users = 0;
        int64_t new_users_30d = 0;
        int64_t threshold = time(0) - 30 * 24 * 3600;

        for (auto& id : ids) {
            auto user = UserMgr::GetInstance()->get(id);
            if (!user || user->getIsDeleted()) {
                continue;
            }
            total_users++;
            if (user->getState() == UserManager::Status::ACTIVE) {
                active_users++;
            } else {
                inactive_users++;
            }
            if (user->getCreateTime() >= threshold) {
                new_users_30d++;
            }
        }

        result->set("total_users", total_users);
        result->set("active_users", active_users);
        result->set("new_users_30d", new_users_30d);
        result->set("inactive_users", inactive_users);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
