#include "user_update_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserUpdateServlet::UserUpdateServlet()
    : BlogServlet("UserUpdateServlet")
{
}

int32_t UserUpdateServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        std::string name = request->getParam("name");
        std::string passwd = request->getParam("passwd");
        std::string bio = request->getParam("bio");
        std::string website = request->getParam("website");
        std::string github = request->getParam("github");
        std::string avatar = request->getParam("avatar");
        std::string old_passwd = request->getParam("old_passwd");

        if (name.empty() && passwd.empty() && bio.empty() && website.empty() && avatar.empty()) {
            result->setErrno(errcode::USER_NO_PARAM);
            break;
        }

        if (!passwd.empty() && passwd.length() < 6) {
            result->setErrno(errcode::USER_INVALID_PASSWORD);
            break;
        }

        auto sdata = getSessionData(request, response);
        int64_t uid = sdata->getData<int64_t>(CookieKey::USER_ID);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        data::UserInfo::ptr info = UserMgr::GetInstance()->get(uid);
        if (!info) {
            result->setErrno(errcode::ACCOUNT_INVALID);
            break;
        }

        // 修改密码时必须提供旧密码校验
        if (!passwd.empty()) {
            if (old_passwd.empty()) {
                result->setErrno(errcode::PARAM_MISSING, "old password required");
                break;
            }
            if (info->getPasswd() != chen::EncryptorUtil::MD5(old_passwd)) {
                result->setErrno(errcode::USER_OLD_PASSWORD_WRONG);
                break;
            }
        }

        if (!name.empty()) {
            info->setName(name);
        }
        if (!passwd.empty()) {
            info->setPasswd(chen::EncryptorUtil::MD5(passwd));
        }
        if (!bio.empty()) {
            info->setBio(bio);
        }
        if (!website.empty()) {
            info->setWebsite(website);
        }
        if (!avatar.empty()) {
            info->setAvatar(avatar);
        }
        if (!github.empty()) {
            info->setGithub(github);
        }
        info->setUpdateTime(time(0));

        auto db = getDB();
        if (!db) {
            result->setErrno(errcode::DB_OPERATION_FAILED);
            break;
        }
        if (data::UserInfoDao::Update(info, db)) {
            result->setErrno(errcode::DB_OPERATION_FAILED, "update user failed");
            ERROR(logger) << "db error errno=" << db->getErrno() << " errstr=" << db->getErrStr();
            break;
        }
        UserMgr::GetInstance()->update(info);
        result->setErrno(errcode::SUCCESS);

        if (!passwd.empty()) {
            int64_t token_time = time(0) + 3600 * 24;
            response->setCookie(CookieKey::USER_ID, EncryptUserId(info->getId()), token_time, "/");
            auto token = UserMgr::GetInstance()->GetToken(info, token_time);
            response->setCookie(CookieKey::TOKEN, token, token_time, "/");
            response->setCookie(CookieKey::TOKEN_TIME, std::to_string(token_time), token_time, "/");
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
