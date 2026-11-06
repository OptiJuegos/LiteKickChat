#ifndef KICKCHAT_WEBSOCKET_HPP
#define KICKCHAT_WEBSOCKET_HPP

#include "kickchat_tls_transport.hpp"
#include "kickchat_websocket_frame.hpp"

class WebSocket {
    TlsTransport &tls;
    websocketFrame::Decoder frames;
    WebSocket(const WebSocket &);
    WebSocket &operator=(const WebSocket &);
    bool sendFrame(int opcode, const std::string &payload);
public:
    explicit WebSocket(TlsTransport &transport);
    bool open(const std::string &host, const std::wstring &wideHost, const std::string &appKey);
    bool sendText(const std::string &text);
    int nextText(std::string &output);
};

#endif
