#ifndef KICKCHAT_HISTORY_HPP
#define KICKCHAT_HISTORY_HPP

#include "kickchat_chat_data.hpp"
#include <string>
#include <vector>
#include <deque>

void loadChatHistory();
void appendChatHistory(const ChatRecord &record);
std::vector<ChatRecord> recentChannelHistory(const std::string &channel,
                                             const std::string &cutoff, size_t limit);
bool findChatUser(const std::wstring &name, ChatRecord &latest, const std::string &channel = "");
std::vector<ChatRecord> recentChatMessages(const std::string &id, size_t limit,
                                            const std::string &channel = "");

#endif
