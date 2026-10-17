#pragma once

#include <chen/http/ws_servlet.h>

namespace blog::servlet {

class PresenceServlet : public chen::http::WSServlet {
public:
    typedef std::shared_ptr<PresenceServlet> ptr;

    PresenceServlet();

    virtual int32_t onConnect(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session) override;
    virtual int32_t onClose(chen::http::HttpRequest::ptr header, chen::http::WSSession::ptr session) override;
    virtual int32_t handle(chen::http::HttpRequest::ptr header, chen::http::WSFrameMessage::ptr msg, chen::http::WSSession::ptr session) override;
};

} // namespace blog::servlet
