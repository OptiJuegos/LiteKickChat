#ifndef KICKCHAT_LINK_TOOLTIP_HPP
#define KICKCHAT_LINK_TOOLTIP_HPP

#include <windows.h>
#include <string>

struct LinkMetadata;

void showLinkTooltip(HWND parent, POINT clientAnchor, const std::wstring &url,
                     const LinkMetadata *metadata, bool loading);
void hideLinkTooltip();
void destroyLinkTooltip();

#endif
