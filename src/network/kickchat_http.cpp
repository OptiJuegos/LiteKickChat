#include "kickchat_http.hpp"
#ifndef KICKCHAT_USE_CURL
#include "kickchat_platform.hpp"
#include <wininet.h>

bool httpGet(const std::wstring &url, std::string &body, size_t maximumBytes) {
    body.clear();
    if (url.compare(0, 8, L"https://") != 0 || maximumBytes > 1024 * 1024)
        return false;
    HINTERNET session = InternetOpenW(L"KickChatWin7/6.5", INTERNET_OPEN_TYPE_PRECONFIG,
                                      NULL, NULL, 0);
    if (!session) return false;
    DWORD timeout = 10000;
    InternetSetOptionW(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    const wchar_t *headers = L"Accept: application/json,image/png,image/gif,*/*\r\n";
    HINTERNET request = InternetOpenUrlW(session, url.c_str(), headers, (DWORD)-1,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI, 0);
    if (!request) { InternetCloseHandle(session); return false; }
    DWORD status = 0, size = sizeof(status);
    bool ok = HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                             &status, &size, NULL) != 0 && status == 200;
    char buffer[8192];
    while (ok) {
        DWORD read = 0;
        if (!InternetReadFile(request, buffer, sizeof(buffer), &read)) {
            ok = false;
            break;
        }
        if (!read) break;
        if (body.size() + read > maximumBytes) { ok = false; break; }
        body.append(buffer, read);
    }
    InternetCloseHandle(request);
    InternetCloseHandle(session);
    if (!ok) body.clear();
    return ok && !body.empty();
}
#endif
