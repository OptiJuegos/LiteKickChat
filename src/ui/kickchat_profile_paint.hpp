#ifndef KICKCHAT_PROFILE_PAINT_HPP
#define KICKCHAT_PROFILE_PAINT_HPP

#include "kickchat_chat_data.hpp"
#include "kickchat_platform.hpp"
#include <string>
#include <vector>

void paintUserProfile(HWND window, const ChatRecord &person,
                      const std::vector<ChatRecord> &messages,
                      const std::wstring &status, HFONT font, HFONT bold,
                      int &scrollOffset);

#endif
