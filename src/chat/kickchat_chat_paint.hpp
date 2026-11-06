#ifndef KICKCHAT_CHAT_PAINT_HPP
#define KICKCHAT_CHAT_PAINT_HPP

#include "kickchat_platform.hpp"
#include "kickchat_text_selection.hpp"

void paintChat(HWND hwnd, HFONT font, HFONT bold,
               const std::wstring &statusText, const std::wstring &authText,
               const std::deque<ChatLine> &lines, int &scroll,
               const std::vector<std::string> &channels, int selected,
               int &maxScroll, int scrollbarWidth, const ChatTextSelection &selection);
bool hasChatLinkAt(int x, int y);
bool chatLinkUrlAt(int x, int y, std::wstring &url);
bool openChatLink(HWND owner, int x, int y);
bool chatUserAt(int x, int y, ChatRecord &record);
int chatRowAt(int x, int y);
bool chatTextAt(int x, int y, ChatTextPosition &position, bool clamp = false);
std::wstring chatMessageText(const ChatLine &line);

#endif
