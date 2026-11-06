#include "kickchat_link_url.hpp"
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <cwctype>
#include <vector>

namespace linkUrl {

namespace {

bool domainHost(const std::wstring &host) {
    if (host.size() < 4 || host.size() > 253 || host.back() == L'.') return false;
    std::wstring lowered = host;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::towlower);
    const wchar_t *blocked[] = {L".localhost", L".local", L".localdomain",
        L".internal", L".lan", L".home", L".test", L".invalid", L".onion"};
    for (size_t i = 0; i < sizeof(blocked) / sizeof(blocked[0]); ++i) {
        std::wstring suffix = blocked[i];
        if (lowered.size() >= suffix.size() &&
            lowered.compare(lowered.size() - suffix.size(), suffix.size(), suffix) == 0)
            return false;
    }
    size_t dot = host.rfind(L'.');
    if (dot == std::wstring::npos || host.size() - dot < 3) return false;
    bool tldLetter = false;
    for (size_t i = dot + 1; i < host.size(); ++i) {
        wchar_t c = host[i];
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) tldLetter = true;
        else if (c < L'0' || c > L'9') return false;
    }
    if (!tldLetter) return false;
    size_t start = 0;
    while (start < host.size()) {
        size_t end = host.find(L'.', start);
        if (end == std::wstring::npos) end = host.size();
        if (end == start || end - start > 63 || host[start] == L'-' || host[end - 1] == L'-')
            return false;
        for (size_t i = start; i < end; ++i) {
            wchar_t c = host[i];
            if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
                  (c >= L'0' && c <= L'9') || c == L'-')) return false;
        }
        start = end + 1;
    }
    return true;
}

} // namespace

bool parse(const std::wstring &url, Parts &parts) {
    if (url.empty() || url.size() > MaxUrlLength) return false;
    for (size_t i = 0; i < url.size(); ++i)
        if (url[i] <= 32 || url[i] == 127 || url[i] == L'\\') return false;
    const size_t sep = url.find(L"://");
    if (sep == std::wstring::npos) return false;
    const size_t authorityEnd = url.find_first_of(L"/?#", sep + 3);
    if (url.substr(sep + 3, authorityEnd - sep - 3).find(L'@') != std::wstring::npos)
        return false;
    URL_COMPONENTS c = {};
    c.dwStructSize = sizeof(c);
    c.dwSchemeLength = c.dwHostNameLength = c.dwUrlPathLength = c.dwExtraInfoLength =
        c.dwUserNameLength = c.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &c)) return false;
    parts.scheme.assign(c.lpszScheme, c.dwSchemeLength);
    parts.secure = CompareStringOrdinal(parts.scheme.c_str(), -1, L"https", -1, TRUE) == CSTR_EQUAL;
    if (!parts.secure &&
        CompareStringOrdinal(parts.scheme.c_str(), -1, L"http", -1, TRUE) != CSTR_EQUAL)
        return false;
    if (c.dwUserNameLength || c.dwPasswordLength || !c.dwHostNameLength) return false;
    parts.host.assign(c.lpszHostName, c.dwHostNameLength);
    if (!domainHost(parts.host)) return false;
    parts.port = c.nPort;
    if (parts.port != (parts.secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT))
        return false;
    parts.path = c.dwUrlPathLength ? std::wstring(c.lpszUrlPath, c.dwUrlPathLength) : L"/";
    if (parts.path.empty()) parts.path = L"/";
    std::wstring extra = c.dwExtraInfoLength ?
                         std::wstring(c.lpszExtraInfo, c.dwExtraInfoLength) : L"";
    extra = extra.substr(0, extra.find(L'#'));
    if (!extra.empty() && extra[0] != L'?') return false;
    parts.target = parts.path + extra;
    parts.origin = (parts.secure ? L"https://" : L"http://") + parts.host;
    return true;
}

std::wstring resolve(const std::wstring &base, const std::wstring &relative) {
    if (relative.empty() || relative.size() > MaxUrlLength) return L"";
    Parts page;
    if (!parse(base, page)) return L"";
    const size_t colon = relative.find(L':');
    const size_t delimiter = relative.find_first_of(L"/?#");
    if (colon != std::wstring::npos &&
        (delimiter == std::wstring::npos || colon < delimiter) &&
        relative.find(L"://") == std::wstring::npos) return L"";
    std::wstring candidate;
    if (relative.find(L"://") != std::wstring::npos) candidate = relative;
    else if (relative.compare(0, 2, L"//") == 0) candidate = page.scheme + L":" + relative;
    else if (relative[0] == L'/') candidate = page.origin + relative;
    else if (relative[0] == L'?') candidate = page.origin + page.path + relative;
    else if (relative[0] == L'#') return L"";
    else {
        size_t slash = page.path.find_last_of(L'/');
        candidate = page.origin + page.path.substr(0, slash + 1) + relative;
    }
    Parts checked;
    if (!parse(candidate, checked)) return L"";
    std::wstring path = checked.path;
    std::vector<std::wstring> segments;
    size_t index = 0;
    while (index < path.size()) {
        size_t next = path.find(L'/', index + 1);
        if (next == std::wstring::npos) next = path.size();
        std::wstring segment = path.substr(index + 1, next - index - 1);
        if (segment == L"..") {
            if (!segments.empty()) segments.pop_back();
        } else if (segment != L".") segments.push_back(segment);
        index = next;
    }
    std::wstring normalized;
    for (size_t i = 0; i < segments.size(); ++i) normalized += L"/" + segments[i];
    if (normalized.empty()) normalized = L"/";
    std::wstring query = checked.target.substr(checked.path.size());
    std::wstring result = checked.origin + normalized + query;
    return result.size() <= MaxUrlLength ? result : L"";
}

} // namespace linkUrl
