#include "kickchat_recent.hpp"
#include <algorithm>

static bool validUtcTimestamp(const std::string &value) {
    if (value.size() < 20 || value.size() > 40 || (value[19] != 'Z' && value[19] != '.' && value[19] != '+'))
        return false;
    for (size_t i = 0; i < 19; ++i) {
        if (i == 4 || i == 7) {
            if (value[i] != '-') return false;
        } else if (i == 10) {
            if (value[i] != 'T' && value[i] != ' ') return false;
        } else if (i == 13 || i == 16) {
            if (value[i] != ':') return false;
        } else if (value[i] < '0' || value[i] > '9') return false;
    }
    return value.find('Z', 19) != std::string::npos ||
           value.compare(value.size() - 6, 6, "+00:00") == 0;
}

bool parseRecentKickMessages(const std::string &json, const std::string &channel,
                             const std::string &utcStart, const std::string &utcEnd,
                             std::vector<ChatRecord> &messages) {
    messages.clear();
    Json root;
    if (!parseJson(json, root)) return false;
    const Json *list = &root;
    if (root.type == Json::OBJECT) {
        list = &root.get("messages");
        if (list->type != Json::ARRAY) {
            const Json &data = root.get("data");
            list = data.type == Json::ARRAY ? &data : &data.get("messages");
            if (list->type == Json::OBJECT) list = &list->get("data");
        }
    }
    if (list->type != Json::ARRAY) return false;
    for (size_t i = 0; i < list->arr.size() && messages.size() < 250; ++i) {
        const Json &item = list->arr[i];
        const std::string type = item.get("type").str();
        if (!type.empty() && type != "message") continue;
        const std::string created = item.get("created_at").str();
        if (!validUtcTimestamp(created)) continue;
        std::string utc = created.substr(0, 19);
        if (utc[10] == ' ') utc[10] = 'T';
        if (utc < utcStart || utc > utcEnd) continue;
        ChatRecord record;
        if (!parseKickChatRecord(item, record)) continue;
        record.channel = channel;
        record.receivedAt = utc;
        messages.push_back(record);
    }
    std::stable_sort(messages.begin(), messages.end(),
                     [](const ChatRecord &a, const ChatRecord &b) {
                         return a.receivedAt < b.receivedAt;
                     });
    return true;
}
