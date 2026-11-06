#ifndef KICKCHAT_CHAT_DATA_HPP
#define KICKCHAT_CHAT_DATA_HPP

#include "kickchat_json.hpp"
#include <string>
#include <vector>

struct ChatBadge {
    std::string type;
    std::string text;
    unsigned count;
    ChatBadge() : count(0) {}
};

struct ChatRecord {
    std::string id;
    std::string messageId;
    std::string user;
    std::string content;
    std::string receivedAt;
    std::string color;
    std::string channel;
    std::vector<ChatBadge> badges;
};

bool parseKickChatRecord(const Json &payload, ChatRecord &record);
std::string encodeChatRecord(const ChatRecord &record);
bool decodeChatRecord(const std::string &text, ChatRecord &record);
std::string badgeLabel(const ChatBadge &badge);

#endif
