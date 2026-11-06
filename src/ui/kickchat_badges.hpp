#ifndef KICKCHAT_BADGES_HPP
#define KICKCHAT_BADGES_HPP

#include "kickchat_platform.hpp"
#include "kickchat_chat_data.hpp"

int badgeRowWidth(HDC dc, const std::vector<ChatBadge> &badges);
void drawBadgeRow(HDC dc, const std::vector<ChatBadge> &badges, int x, int y, int right);

#endif
