#ifndef KICKCHAT_PROFILE_HPP
#define KICKCHAT_PROFILE_HPP

#include "kickchat_chat_data.hpp"
#include "kickchat_platform.hpp"

void showUserProfile(HWND owner, const ChatRecord &record);
void refreshUserProfile(const ChatRecord &record);
void completeUserProfileAction(const SendResult &result);

#endif
