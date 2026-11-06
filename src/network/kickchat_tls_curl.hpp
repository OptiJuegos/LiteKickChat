#ifndef KICKCHAT_TLS_CURL_HPP
#define KICKCHAT_TLS_CURL_HPP

#include "kickchat_tls_transport.hpp"
#include <curl/curl.h>

class CurlTlsTransport : public TlsTransport {
    CURL *handle;
    bool connected;
    CurlTlsTransport(const CurlTlsTransport &);
    CurlTlsTransport &operator=(const CurlTlsTransport &);
public:
    CurlTlsTransport();
    ~CurlTlsTransport();
    bool connectTls(const char *host, const wchar_t *hostname) override;
    bool write(const std::string &data) override;
    int readPlain(std::string &output) override;
};

#endif
