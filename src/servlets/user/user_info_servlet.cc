#include "user_info_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_follow_rel_manager.h"
#include "../../manager/user_manager.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserInfoServlet::UserInfoServlet()
    : BlogServlet("UserInfoServlet")
{
}

int32_t UserInfoServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        auto sdata = getSessionData(request, response);
        int64_t uid = sdata->getData<int64_t>(CookieKey::USER_ID);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        int64_t id = request->getParamAs<int64_t>("user_id", -1);
        if (id != -1) {
            uid = id;
        }

        data::UserInfo::ptr info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }
        result->setErrno(errcode::SUCCESS);
        result->set("id", info->getId());
        result->set("name", info->getName());
        result->set("account", info->getAccount());
        result->set("avatar", info->getAvatar());
        result->set("email", info->getEmail());
        result->set("role", info->getRole());
        result->set("bio", info->getBio());
        result->set("website", info->getWebsite());
        result->set("location", info->getLocation());
        result->set("status", info->getIsDeleted());
        result->set("github", info->getGithub());
        result->set("login_time", info->getLoginTime());
        result->set("create_time", info->getCreateTime());
        result->set("following_count", UserFollowRelMgr::GetInstance()->countFollowing(uid));
        result->set("follower_count", UserFollowRelMgr::GetInstance()->countFollowers(uid));
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
