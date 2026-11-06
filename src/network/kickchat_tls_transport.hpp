#ifndef KICKCHAT_TLS_TRANSPORT_HPP
#define KICKCHAT_TLS_TRANSPORT_HPP

#include <string>

class TlsTransport {
public:
    virtual ~TlsTransport() {}
    virtual bool connectTls(const char *host, const wchar_t *hostname) = 0;
    virtual bool write(const std::string &data) = 0;
    virtual int readPlain(std::string &output) = 0;
};

#endif
