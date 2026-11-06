#ifndef KICKCHAT_EMOTE_RENDER_HPP
#define KICKCHAT_EMOTE_RENDER_HPP

#include "kickchat_platform.hpp"
#include "kickchat_link_paint.hpp"
#include <string>

int renderChatContent(HDC dc, const std::string &content, int left, int top,
                      int right, bool draw, const RECT *viewport = NULL,
                      std::vector<TextLinkHit> *links = NULL,
                      std::vector<ChatTextGlyph> *glyphs = NULL);

#endif
