#include "user_admin_reset_passwd_servlet.h"

#include <chen/log/log.h>

#include "../../event/event_define.h"
#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserAdminResetPasswdServlet::UserAdminResetPasswdServlet()
    : BlogLoginedServlet("UserAdminResetPasswdServlet")
{
}

int32_t UserAdminResetPasswdServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, id, "id");
        DEFINE_AND_CHECK_STRING(result, passwd_f, "passwd_f");
        DEFINE_AND_CHECK_STRING(result, passwd_s, "passwd_s");

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

        if (passwd_f.empty() || passwd_s.empty()) {
            result->setErrno(errcode::PARAM_MISSING, "password required");
            break;
        }

        if (passwd_f != passwd_s) {
            result->setErrno(errcode::USER_PASSWORDS_DIFFER);
            break;
        }

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_CONNECTION_FAILED);
            break;
        }

        chen::ITransaction::ptr trans = db->openTransaction();
        data::UserInfo::ptr info = UserMgr::GetInstance()->get(id);
        if (!info) {
            result->setErrno(errcode::USER_NOT_FOUND);
            break;
        }
        info->setPasswd(chen::EncryptorUtil::MD5(passwd_s));

        if (data::UserInfoDao::Update(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "insert user failed");
            break;
        }
        trans->commit();
        UserMgr::GetInstance()->update(info);

        // Notify affected user
        {
            EventUserAdminData data = {};
            data.type = "password_reset";
            data.user_id = id;

            chen::EventBusMgr::GetInstance()->emitAsync(EVENT_ID_USER_ADMIN, std::move(data));
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
