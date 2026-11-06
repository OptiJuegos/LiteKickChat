#ifndef KICKCHAT_LINK_METADATA_HPP
#define KICKCHAT_LINK_METADATA_HPP

#include <windows.h>
#include <string>
#include <vector>

struct LinkMetadata {
    std::wstring title, description, siteName, imageUrl, url;
    std::vector<BYTE> imageBytes;
    bool success;
    LinkMetadata():success(false){}
};

bool fetchLinkMetadata(const std::wstring &url, LinkMetadata &out);
bool parseLinkMetadata(const std::string &html, const std::wstring &url, LinkMetadata &out);

#endif
