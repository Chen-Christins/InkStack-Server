#include "user_admin_delete_servlet.h"

#include <chen/log/log.h>

#include "../../event/event_define.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserAdminDeleteServlet::UserAdminDeleteServlet()
    : BlogLoginedServlet("UserAdminDeleteServlet")
{
}

int32_t UserAdminDeleteServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, user_id, "user_id");

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

        auto info = UserMgr::GetInstance()->get(user_id);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        if (info->getIsDeleted()) {
            result->setErrno(errcode::USER_ALREADY_DELETED);
            break;
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }
        auto trans = db->openTransaction();
        if (!trans) {
            result->setErrno(errcode::DB_TRANSACTION_FAILED);
            break;
        }
        time_t now = time(0);
        info->setIsDeleted(1);
        info->setState(UserManager::Status::INACTIVE);
        info->setUpdateTime(now);
        if (data::UserInfoDao::Update(info, db)) {
            ERROR(logger) << "update user fail";
            result->setErrno(errcode::DB_OPERATION_FAILED, "delete user failed");
            break;
        }
        if (!trans->commit()) {
            ERROR(logger) << "commit fail";
            info->setIsDeleted(0);
            result->setErrno(errcode::DB_COMMIT_FAILED);
            break;
        }
        UserMgr::GetInstance()->update(info);
        result->set("user_id", user_id);

        // Notify affected user
        {
            EventUserAdminData data = {};
            data.type = "account_deleted";
            data.user_id = user_id;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_USER_ADMIN, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
