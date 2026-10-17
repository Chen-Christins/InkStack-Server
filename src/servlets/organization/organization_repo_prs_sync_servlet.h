/**
 * @file organization_repo_prs_sync_servlet.h
 * @brief 组织仓库 PR 同步接口（repo_id 全量 / id 单条）
 * @author Christins
 * @date 2026-06-24
 * @copyright Apache 2.0
 */
#pragma once

#include "../../struct.h"

namespace blog {
namespace servlet {

class OrganizationRepoPrsSyncServlet : public BlogLoginedServlet {
public:
    typedef std::shared_ptr<OrganizationRepoPrsSyncServlet> ptr;
    OrganizationRepoPrsSyncServlet();

protected:
    int32_t handle(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        Result::ptr result) override;
};

} // namespace servlet
} // namespace blog
