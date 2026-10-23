/**
 * @file auth_service.h
 * @brief 认证服务 — 统一处理会话签发、解析、撤销与旧 token 兼容
 * @author Christins
 * @date 2026-10-23
 * @copyright Apache 2.0
 */
#pragma once

#include <string>

#include <chen/http/http.h>
#include <chen/http/http_session.h>

#include "auth_context.h"

namespace blog {

/**
 * @brief 会话签发选项
 */
struct IssueOptions {
    /// 是否更新用户最近登录时间
    bool update_login_time = true;
};

class AuthService {
public:
    /**
     * @brief 解析当前请求的登录态
     * @param request HTTP 请求
     * @param response 用于清理失效 Cookie，可为空
     * @param touch 是否刷新会话活跃时间
     */
    static AuthContext Resolve(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response = nullptr, bool touch = true);

    /**
     * @brief 用旧版 S_UID / S_TOKEN Cookie 换取新的会话 Cookie
     * @return 是否成功升级
     */
    static bool UpgradeLegacySession(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session);

    /**
     * @brief 登录成功后签发会话
     * @return 0 成功，否则返回业务错误码
     */
    static int32_t IssueSession(
        chen::http::HttpRequest::ptr request,
        chen::http::HttpResponse::ptr response,
        chen::http::HttpSession::ptr session,
        data::UserInfo::ptr user,
        const IssueOptions& options = IssueOptions());

    /**
     * @brief 撤销当前会话（登出）
     */
    static void RevokeSession(chen::http::HttpRequest::ptr request, chen::http::HttpResponse::ptr response, const AuthContext& ctx);

    /**
     * @brief 撤销用户全部会话（改密、封禁、删除等场景）
     */
    static void RevokeAllSessions(int64_t uid);

    /**
     * @brief WebSocket 握手认证：优先 Cookie 会话，兼容旧 query token
     */
    static bool ResolveWs(chen::http::HttpRequest::ptr request, int64_t& uid, data::UserInfo::ptr& user);

    /**
     * @brief 写入 SESSIONID Cookie（HttpOnly / SameSite）
     */
    static void SetSessionCookie(chen::http::HttpResponse::ptr response, const std::string& session_id, int64_t max_age);

    /**
     * @brief 清除 SESSIONID Cookie
     */
    static void ClearSessionCookie(chen::http::HttpResponse::ptr response);

    /**
     * @brief 将会话映射到框架 SessionData，兼容 2FA 临时密钥等旧逻辑
     */
    static void MirrorSessionData(const std::string& session_id, int64_t uid);
};

} // namespace blog
