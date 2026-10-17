#include "user_device_kick_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_login_device_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserDeviceKickServlet::UserDeviceKickServlet()
    : BlogLoginedServlet("UserDeviceKickServlet")
{
}

int32_t UserDeviceKickServlet::handle(
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

        DEFINE_AND_CHECK_STRING(result, device_id, "device_id");

        auto device = UserLoginDeviceMgr::GetInstance()->getActiveByDevice(device_id);
        if (!device) {
            result->setErrno(errcode::DEVICE_NOT_FOUND);
            break;
        }
        if (device->getUserId() != uid) {
            result->setErrno(errcode::ACCESS_DENIED);
            break;
        }

        if (UserLoginDeviceMgr::GetInstance()->kick(device)) {
            result->setErrno(errcode::DEVICE_KICK_FAILED);
            break;
        }
        result->setErrno(errcode::SUCCESS);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
