#include "user_device_list_servlet.h"

#include <chen/log/log.h>

#include "../../manager/user_login_device_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

UserDeviceListServlet::UserDeviceListServlet()
    : BlogLoginedServlet("UserDeviceListServlet")
{
}

int32_t UserDeviceListServlet::handle(
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

        // 当前请求设备标识（用于标记“当前设备”）
        std::string cur_device_id = request->getHeader("X-Device-Id");
        if (cur_device_id.empty()) {
            cur_device_id = request->getCookie(CookieKey::DEVICE_ID);
        }

        std::vector<data::UserLoginDeviceInfo::ptr> devices;
        UserLoginDeviceMgr::GetInstance()->listActiveByUser(devices, uid);

        result->setErrno(errcode::SUCCESS);
        result->set("total", (int64_t)devices.size());
        auto& list = result->jsondata["list"];
        for (const auto& d : devices) {
            Json::Value item;
            item["id"] = d->getId();
            item["device_id"] = d->getDeviceId();
            item["device_name"] = d->getDeviceName();
            item["platform"] = d->getPlatform();
            item["ip"] = d->getIp();
            item["is_active"] = d->getIsActive();
            item["is_current"] = (!cur_device_id.empty() && cur_device_id == d->getDeviceId());
            item["login_time"] = d->getLoginTime();
            item["last_active_time"] = d->getLastActiveTime();
            list.append(item);
        }
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
