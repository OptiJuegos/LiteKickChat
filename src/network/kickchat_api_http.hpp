#ifndef KICKCHAT_API_HTTP_HPP
#define KICKCHAT_API_HTTP_HPP

#include "kickchat_platform.hpp"
#include <string>

struct HttpResult {
    DWORD status;
    std::string body, error;
    HttpResult() : status(0) {}
};

inline std::string formEncode(const std::string &s) {
    static const char *hex="0123456789ABCDEF";std::string out;
    for(size_t i=0;i<s.size();i++){unsigned char c=(unsigned char)s[i];
        if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='.'||c=='_'||c=='~')out+=(char)c;
        else {out+='%';out+=hex[c>>4];out+=hex[c&15];}
    }return out;
}

HttpResult kickHttp(const wchar_t *host, const wchar_t *path, const wchar_t *method,
                    const std::string &body, const std::string &bearer, bool form);
std::wstring apiError(const HttpResult &result);

#endif
