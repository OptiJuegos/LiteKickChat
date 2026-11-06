#include <winsock2.h>
#include <ws2tcpip.h>
#include "kickchat_link_metadata.hpp"
#include "kickchat_link_url.hpp"
#include <winhttp.h>
#include <algorithm>
#include <cwctype>

namespace {

const size_t MAX_HTML_BYTES = 512 * 1024;
const size_t MAX_IMAGE_BYTES = 1024 * 1024;

bool publicIp(const sockaddr *address) {
    if (address->sa_family == AF_INET) {
        const unsigned char *b = reinterpret_cast<const unsigned char *>(
            &reinterpret_cast<const sockaddr_in *>(address)->sin_addr);
        const unsigned a = b[0], c = b[1], d = b[2];
        if (a == 0 || a == 10 || a == 127 || a >= 224 ||
            (a == 100 && c >= 64 && c <= 127) ||
            (a == 169 && c == 254) ||
            (a == 172 && c >= 16 && c <= 31) ||
            (a == 192 && (c == 168 || c == 0 || (c == 88 && d == 99))) ||
            (a == 198 && (c == 18 || c == 19 || (c == 51 && d == 100))) ||
            (a == 203 && c == 0 && d == 113)) return false;
        return true;
    }
    if (address->sa_family == AF_INET6) {
        const unsigned char *b = reinterpret_cast<const unsigned char *>(
            &reinterpret_cast<const sockaddr_in6 *>(address)->sin6_addr);
        if ((b[0] & 0xe0) != 0x20 || (b[0] == 0x20 && b[1] == 0x02) ||
            (b[0] == 0x20 && b[1] == 0x01 && b[2] == 0 && b[3] == 0))
            return false;
        if (b[0] == 0x20 && b[1] == 0x01 && b[2] == 0x0d && b[3] == 0xb8)
            return false;
        return true;
    }
    return false;
}

bool publicHost(const std::wstring &host) {
    WSADATA wsa = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    addrinfoW hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfoW *addresses = NULL;
    int status = GetAddrInfoW(host.c_str(), NULL, &hints, &addresses);
    bool valid = status == 0 && addresses != NULL;
    for (addrinfoW *p = addresses; p; p = p->ai_next)
        if (!p->ai_addr || !publicIp(p->ai_addr)) valid = false;
    if (addresses) FreeAddrInfoW(addresses);
    WSACleanup();
    return valid;
}

std::wstring responseHeader(HINTERNET request, DWORD type) {
    wchar_t buffer[4096] = {};
    DWORD size = sizeof(buffer);
    if (!WinHttpQueryHeaders(request, type, WINHTTP_HEADER_NAME_BY_INDEX,
                             buffer, &size, WINHTTP_NO_HEADER_INDEX)) return L"";
    size_t count = size / sizeof(wchar_t);
    while (count && buffer[count - 1] == L'\0') --count;
    return std::wstring(buffer, count);
}

bool getHttp(const std::wstring &url, size_t maximum, bool image,
             std::string &body, std::wstring &finalUrl) {
    body.clear();
    finalUrl.clear();
    std::wstring current = url;
    const DWORD started = GetTickCount();
    for (unsigned redirects = 0; redirects <= 3; ++redirects) {
        if (GetTickCount() - started >= 5000) return false;
        linkUrl::Parts parts;
        if (!linkUrl::parse(current, parts) || (image && !parts.secure) || !publicHost(parts.host))
            return false;
        DWORD elapsed = GetTickCount() - started;
        if (elapsed >= 5000) return false;
        HINTERNET session = WinHttpOpen(L"KickChatWin7-LinkPreview/1.0",
            WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return false;
        DWORD tls = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
        WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &tls, sizeof(tls));
        int remaining = static_cast<int>(5000 - elapsed);
        WinHttpSetTimeouts(session, remaining, remaining, remaining, remaining);
        HINTERNET connection = WinHttpConnect(session, parts.host.c_str(), parts.port, 0);
        HINTERNET request = connection ?
            WinHttpOpenRequest(connection, L"GET", parts.target.c_str(), NULL,
                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                parts.secure ? WINHTTP_FLAG_SECURE : 0) : NULL;
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        DWORD disable = WINHTTP_DISABLE_COOKIES;
        bool ok = request &&
            WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)) &&
            WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));
        if (ok && GetTickCount() - started < 5000) {
            const wchar_t *headers = image ?
                L"Accept: image/png,image/jpeg,image/gif,image/webp\r\nAccept-Encoding: identity\r\n" :
                L"Accept: text/html,application/xhtml+xml\r\nAccept-Encoding: identity\r\n";
            ok = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1),
                                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (ok && GetTickCount() - started < 5000)
                ok = WinHttpReceiveResponse(request, NULL);
            else ok = false;
        } else ok = false;
        DWORD status = 0, length = sizeof(status);
        if (ok) ok = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE |
            WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
            &status, &length, WINHTTP_NO_HEADER_INDEX) != 0;
        std::wstring location, contentType, contentEncoding;
        if (ok) {
            if (status == 301 || status == 302 || status == 303 ||
                status == 307 || status == 308)
                location = responseHeader(request, WINHTTP_QUERY_LOCATION);
            else if (status == 200) {
                contentType = responseHeader(request, WINHTTP_QUERY_CONTENT_TYPE);
                contentEncoding = responseHeader(request, WINHTTP_QUERY_CONTENT_ENCODING);
                DWORD contentLength = 0;
                length = sizeof(contentLength);
                if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH |
                    WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                    &contentLength, &length, WINHTTP_NO_HEADER_INDEX) &&
                    contentLength > maximum) ok = false;
                std::wstring lowered = contentType;
                std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::towlower);
                bool allowed = lowered.empty() ? !image :
                    (image ? lowered.find(L"image/png") == 0 ||
                             lowered.find(L"image/jpeg") == 0 ||
                             lowered.find(L"image/gif") == 0 ||
                             lowered.find(L"image/webp") == 0 :
                             lowered.find(L"text/html") == 0 ||
                             lowered.find(L"application/xhtml+xml") == 0);
                if (!allowed || (!contentEncoding.empty() &&
                    contentEncoding != L"identity")) ok = false;
                char buffer[8192];
                while (ok) {
                    if (GetTickCount() - started >= 5000) { ok = false; break; }
                    DWORD read = 0;
                    if (!WinHttpReadData(request, buffer, sizeof(buffer), &read)) {
                        ok = false; break;
                    }
                    if (!read) break;
                    if (body.size() + read > maximum) { ok = false; break; }
                    body.append(buffer, read);
                }
            } else ok = false;
        }
        if (request) WinHttpCloseHandle(request);
        if (connection) WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        if (!ok) return false;
        if (!location.empty()) {
            current = linkUrl::resolve(current, location);
            if (current.empty() || redirects == 3) return false;
            body.clear();
            continue;
        }
        if (status != 200 || body.empty()) return false;
        finalUrl = current;
        return true;
    }
    return false;
}

bool imageSignature(const std::string &bytes) {
    if (bytes.size() >= 8 &&
        bytes.compare(0, 8, "\x89PNG\r\n\x1a\n", 8) == 0) return true;
    if (bytes.size() >= 3 && bytes.compare(0, 3, "\xff\xd8\xff", 3) == 0) return true;
    if (bytes.size() >= 6 &&
        (bytes.compare(0, 6, "GIF87a") == 0 || bytes.compare(0, 6, "GIF89a") == 0)) return true;
    if (bytes.size() >= 12 && bytes.compare(0, 4, "RIFF") == 0 &&
        bytes.compare(8, 4, "WEBP") == 0) return true;
    return false;
}

} // namespace

bool fetchLinkMetadata(const std::wstring &url, LinkMetadata &out) {
    out = LinkMetadata();
    std::string html;
    std::wstring resolved;
    if (!getHttp(url, MAX_HTML_BYTES, false, html, resolved) ||
        !parseLinkMetadata(html, resolved, out)) return false;
    if (!out.imageUrl.empty()) {
        std::string bytes;
        std::wstring imageUrl;
        if (getHttp(out.imageUrl, MAX_IMAGE_BYTES, true, bytes, imageUrl) &&
            imageSignature(bytes))
            out.imageBytes.assign(bytes.begin(), bytes.end());
    }
    return out.success;
}
