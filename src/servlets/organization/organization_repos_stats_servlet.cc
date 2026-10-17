#include "organization_repos_stats_servlet.h"

#include <chen/log/log.h>

#include "../../manager/organization_manager.h"
#include "../../manager/organization_repo_manager.h"
#include "../../util.h"

namespace blog {
namespace servlet {

static chen::Logger::ptr logger = LOG_ROOT();

OrganizationReposStatsServlet::OrganizationReposStatsServlet()
    : BlogLoginedServlet("OrganizationReposStatsServlet")
{
}

int32_t OrganizationReposStatsServlet::handle(
    chen::http::HttpRequest::ptr request,
    chen::http::HttpResponse::ptr response,
    chen::http::HttpSession::ptr session,
    Result::ptr result)
{
    do {
        DEFINE_AND_CHECK_TYPE(result, int64_t, org_id, "org_id");

        int64_t uid = getUserId(request);
        if (!uid) {
            result->setErrno(errcode::NOT_LOGIN);
            break;
        }

        // 检查组织是否存在
        auto org = OrganizationMgr::GetInstance()->get(org_id);
        if (!org) {
            result->setErrno(errcode::ORG_NOT_FOUND);
            break;
        }

        int64_t total = OrganizationRepoMgr::GetInstance()->getCountByOrg(org_id);

        result->set("total_repos", total);
    } while (0);
    response->setBody(result->toJsonString());
    return 0;
}

} // namespace servlet
} // namespace blog
