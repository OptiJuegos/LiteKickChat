#ifndef KICKCHAT_TLS_SCHANNEL_HPP
#define KICKCHAT_TLS_SCHANNEL_HPP

#include "kickchat_platform.hpp"
#include "kickchat_tls_transport.hpp"

class Tls : public TlsTransport {
    SOCKET sock;
    CredHandle cred;
    CtxtHandle ctx;
    bool hasCred, hasCtx;
    SecPkgContext_StreamSizes sizes;
    std::vector<char> encrypted;
    enum { TIMEOUT = 0, OK = 1, FAIL = -1 };
    int receiveRaw(std::vector<char> &destination);
    Tls(const Tls &);
    Tls &operator=(const Tls &);
public:
    Tls();
    ~Tls();
    void closeTls();
    bool connectTls(const char *host, const wchar_t *hostname) override;
    bool write(const std::string &data) override;
    int readPlain(std::string &output) override;
};

#endif
