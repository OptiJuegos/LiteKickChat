#ifndef KICKCHAT_CHAT_EVENTS_HPP
#define KICKCHAT_CHAT_EVENTS_HPP

#include "kickchat_channel_views.hpp"

void receiveChatPacket(const UiPacket &packet, ChannelViews &views,
                       std::deque<ChatLine> &visible, ChatTextSelection &selection, int &scroll);
void receiveChannelIdPacket(const UiPacket &packet, ChannelViews &views, HWND sendButton);
void receiveStatusPacket(const UiPacket &packet, ChannelViews &views, std::wstring &status);
void receiveSendResult(const SendResult &result, ChannelViews &views,
                       std::wstring &authStatus, HWND messageEdit);

#endif
