#ifndef KICKCHAT_LINK_PREVIEW_HPP
#define KICKCHAT_LINK_PREVIEW_HPP

#include <string>

struct LinkPreview {
    std::string url;
    std::string host;
};

struct LinkSpan {
    size_t start;
    size_t length;
    LinkPreview preview;
};

bool findNextLink(const std::string &message, size_t from, LinkSpan &match);
bool findLinkPreview(const std::string &message, LinkPreview &preview);

#endif
