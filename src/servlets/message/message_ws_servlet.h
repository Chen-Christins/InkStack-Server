#pragma once

#include <chen/http/ws_servlet.h>

namespace blog {
namespace servlet {

class MessageWSServlet : public chen::http::WSServlet {
public:
    MessageWSServlet();

    int32_t onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session) override;

    int32_t onClose(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session) override;

    int32_t handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session) override;
};

} // namespace servlet
} // namespace blog