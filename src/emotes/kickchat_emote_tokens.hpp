#ifndef KICKCHAT_EMOTE_TOKENS_HPP
#define KICKCHAT_EMOTE_TOKENS_HPP

#include <map>
#include <string>
#include <vector>

struct EmoteToken {
    std::string text;
    std::string imageKey;
    std::string imageUrl;
};

struct EmoteDefinition {
    std::string id;
    std::string url;
};

typedef std::map<std::string, EmoteDefinition> EmoteCatalog;

std::vector<EmoteToken> splitEmoteMessage(const std::string &content,
                                           const EmoteCatalog &catalog);
bool parseSevenTvSet(const std::string &json, EmoteCatalog &catalog);

#endif
