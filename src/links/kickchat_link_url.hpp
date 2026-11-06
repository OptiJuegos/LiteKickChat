#ifndef KICKCHAT_LINK_URL_HPP
#define KICKCHAT_LINK_URL_HPP

#include <cstddef>
#include <string>

namespace linkUrl {

static const size_t MaxUrlLength = 2048;

struct Parts {
    std::wstring scheme, host, target, path, origin;
    unsigned short port;
    bool secure;
};

bool parse(const std::wstring &url, Parts &parts);
std::wstring resolve(const std::wstring &base, const std::wstring &relative);

} // namespace linkUrl

#endif
