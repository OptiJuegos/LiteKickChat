#include "kickchat_chat_data.hpp"
#include <cstdlib>
#include <cstdio>
#include <cerrno>

static bool validId(const std::string &id) {
    if (id.empty() || id.size() > 20) return false;
    for (size_t i = 0; i < id.size(); ++i)
        if (id[i] < '0' || id[i] > '9') return false;
    if (id == "0") return false;
    errno = 0;
    char *end = NULL;
    std::strtoull(id.c_str(), &end, 10);
    return errno != ERANGE && end && *end == '\0';
}

static std::vector<ChatBadge> readBadges(const Json &identity) {
    const Json *list = &identity.get("badges");
    if (list->type != Json::ARRAY) list = &identity.get("badges_v2");
    std::vector<ChatBadge> badges;
    if (list->type != Json::ARRAY) return badges;
    for (size_t i = 0; i < list->arr.size() && badges.size() < 8; ++i) {
        const Json &item = list->arr[i];
        if (item.type != Json::OBJECT) continue;
        ChatBadge badge;
        badge.type = item.get("type").str();
        badge.text = item.get("text").str();
        if (badge.type.empty() || badge.type.size() > 48) continue;
        if (badge.text.size() > 48) badge.text.resize(48);
        const std::string count = item.get("count").str();
        if (!count.empty() && count.size() <= 5 && count.find_first_not_of("0123456789") == std::string::npos)
            badge.count = static_cast<unsigned>(std::strtoul(count.c_str(), NULL, 10));
        badges.push_back(badge);
    }
    return badges;
}

bool parseKickChatRecord(const Json &payload, ChatRecord &record) {
    const Json &sender = payload.get("sender");
    if (sender.type != Json::OBJECT) return false;
    ChatRecord next;
    next.id = sender.get("id").str();
    next.messageId = payload.get("id").str();
    if (next.messageId.empty()) next.messageId = payload.get("message_id").str();
    if (next.messageId.size() > 128) next.messageId.clear();
    next.user = sender.get("username").str();
    next.content = payload.get("content").str();
    if (!validId(next.id) || next.user.empty() || next.user.size() > 160 ||
        next.content.empty()) return false;
    if (next.content.size() > 8192) next.content.resize(8192);
    const Json &identity = sender.get("identity");
    next.color = identity.get("color").str();
    if (next.color.size() != 7 || next.color[0] != '#') next.color = "#4bdc7d";
    next.badges = readBadges(identity);
    record = next;
    return true;
}

std::string encodeChatRecord(const ChatRecord &record) {
    std::string out = "{\"id\":" + jsonEscape(record.id) +
        ",\"message_id\":" + jsonEscape(record.messageId) +
        ",\"user\":" + jsonEscape(record.user) +
        ",\"content\":" + jsonEscape(record.content) +
        ",\"received_at\":" + jsonEscape(record.receivedAt) +
        ",\"color\":" + jsonEscape(record.color) +
        ",\"channel\":" + jsonEscape(record.channel) + ",\"badges\":[";
    for (size_t i = 0; i < record.badges.size(); ++i) {
        if (i) out += ',';
        const ChatBadge &badge = record.badges[i];
        char count[20]; std::sprintf(count, "%u", badge.count);
        out += "{\"type\":" + jsonEscape(badge.type) +
            ",\"text\":" + jsonEscape(badge.text) + ",\"count\":" + count + "}";
    }
    return out + "]}";
}

bool decodeChatRecord(const std::string &text, ChatRecord &record) {
    Json data;
    if (text.size() > 12000 || !parseJson(text, data) || data.type != Json::OBJECT) return false;
    ChatRecord next;
    next.id = data.get("id").str();
    next.messageId = data.get("message_id").str();
    if (next.messageId.size() > 128) return false;
    next.user = data.get("user").str();
    next.content = data.get("content").str();
    next.receivedAt = data.get("received_at").str();
    next.color = data.get("color").str();
    next.channel = data.get("channel").str();
    if (next.channel.size() > 32) return false;
    if (!validId(next.id) || next.user.empty() || next.user.size() > 160 ||
        next.content.empty() || next.content.size() > 8192 || next.receivedAt.size() != 19) return false;
    next.badges = readBadges(data);
    record = next;
    return true;
}

std::string badgeLabel(const ChatBadge &badge) {
    if (badge.type == "moderator") return "MOD";
    if (badge.type == "broadcaster") return "LIVE";
    if (badge.type == "subscriber") {
        if (badge.count) { char n[24]; std::sprintf(n, "SUB %u", badge.count); return n; }
        return "SUB";
    }
    if (badge.type == "vip") return "VIP";
    if (badge.type == "og") return "OG";
    if (badge.type == "sub_gifter") {
        if (badge.count) { char n[24]; std::sprintf(n, "GIFT x%u", badge.count); return n; }
        return "GIFT";
    }
    if (badge.type == "verified") return "VERIFIED";
    if (!badge.text.empty()) return badge.text.substr(0, 12);
    return badge.type.substr(0, 12);
}
