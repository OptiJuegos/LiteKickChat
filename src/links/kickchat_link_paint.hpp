#ifndef KICKCHAT_LINK_PAINT_HPP
#define KICKCHAT_LINK_PAINT_HPP

#include "kickchat_platform.hpp"

struct TextLinkHit {
    RECT area;
    std::wstring url;
};

struct ChatTextGlyph {
    RECT area;
    size_t offset;
    size_t length;
};

void appendChatTextGlyphs(HDC dc, const std::wstring &text, int x, int y,
                          int height, size_t offset, const RECT *viewport,
                          std::vector<ChatTextGlyph> *glyphs);

int renderLinkedText(HDC dc, const std::string &content, int left, int top,
                     int right, bool draw, const RECT &viewport,
                     std::vector<TextLinkHit> *hits,
                     std::vector<ChatTextGlyph> *glyphs = NULL);

#endif
