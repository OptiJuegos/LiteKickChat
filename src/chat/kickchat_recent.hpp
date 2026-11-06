#ifndef KICKCHAT_RECENT_HPP
#define KICKCHAT_RECENT_HPP

#include "kickchat_chat_data.hpp"
#include <string>
#include <vector>

bool parseRecentKickMessages(const std::string &json, const std::string &channel,
                             const std::string &utcStart, const std::string &utcEnd,
                             std::vector<ChatRecord> &messages);

#ifdef _WIN32
void fetchRecentKickMessages(const std::string &channel, unsigned chatroomId);
#endif

#endif
