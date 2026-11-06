#include "kickchat_emote_tokens.hpp"
#include "kickchat_json.hpp"

static bool validKey(const std::string &id, bool numeric) {
    if (id.empty() || id.size() > 48) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        if ((c >= '0' && c <= '9') || (!numeric && ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z')))) continue;
        return false;
    }
    return true;
}

static bool whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

std::vector<EmoteToken> splitEmoteMessage(const std::string &content,
                                           const EmoteCatalog &catalog) {
    std::vector<EmoteToken> result;
    for (size_t at = 0; at < content.size();) {
        EmoteToken token;
        if (result.size() == 2047) {
            token.text = content.substr(at);
            result.push_back(token);
            break;
        }
        if (content.compare(at, 7, "[emote:") == 0) {
            size_t colon = content.find(':', at + 7);
            size_t close = content.find(']', at + 7);
            if (colon != std::string::npos && close != std::string::npos && colon < close &&
                close - at < 130 && validKey(content.substr(at + 7, colon - at - 7), true)) {
                const std::string id = content.substr(at + 7, colon - at - 7);
                const std::string name = content.substr(colon + 1, close - colon - 1);
                if (!name.empty() && name.size() < 96) {
                    token.text = ":" + name + ":";
                    token.imageKey = "kick:" + id;
                    token.imageUrl = "https://files.kick.com/emotes/" + id + "/fullsize";
                    at = close + 1;
                    result.push_back(token);
                    continue;
                }
            }
        }
        size_t end = at + 1;
        const bool space = whitespace(content[at]);
        while (end < content.size() && whitespace(content[end]) == space &&
               content.compare(end, 7, "[emote:") != 0) ++end;
        token.text = content.substr(at, end - at);
        if (!space) {
            EmoteCatalog::const_iterator match = catalog.find(token.text);
            if (match != catalog.end()) {
                token.imageKey = "7tv:" + match->second.id;
                token.imageUrl = match->second.url;
            }
        }
        result.push_back(token);
        at = end;
    }
    return result;
}

bool parseSevenTvSet(const std::string &text, EmoteCatalog &catalog) {
    Json root;
    if (!parseJson(text, root) || root.type != Json::OBJECT) return false;
    const Json &set = root.get("emote_set").type == Json::OBJECT ? root.get("emote_set") : root;
    const Json &emotes = set.get("emotes");
    if (emotes.type != Json::ARRAY) return false;
    for (size_t i = 0; i < emotes.arr.size() && i < 1500; ++i) {
        const Json &entry = emotes.arr[i];
        const std::string name = entry.get("name").str();
        const Json &data = entry.get("data");
        std::string id = entry.get("id").str();
        if (id.empty()) id = data.get("id").str();
        if (name.empty() || name.size() > 90 || !validKey(id, false)) continue;
        // Prefer formats supported by Windows 7 GDI+, using actual CDN filenames.
        const bool animated = data.get("animated").str() == "true";
        const Json &host = data.get("host");
        const Json &files = host.get("files");
        const std::string rawHost = host.get("url").str();
        std::string file;
        if (files.type == Json::ARRAY) {
            for (size_t k = 0; k < files.arr.size(); ++k) {
                const std::string filename = files.arr[k].get("name").str();
                const bool gif = filename.size() >= 4 && filename.substr(filename.size()-4) == ".gif";
                const bool png = filename.size() >= 4 && filename.substr(filename.size()-4) == ".png";
                if (animated ? gif : png) {
                    if (file.empty() || filename.compare(0, 2, "1x") == 0) file = filename;
                }
            }
        }
        EmoteDefinition emote;
        emote.id = id;
        const std::string expected = "//cdn.7tv.app/emote/" + id;
        if ((rawHost == expected || rawHost == "https:" + expected) &&
            !file.empty() && file.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") == std::string::npos)
            emote.url = "https://cdn.7tv.app/emote/" + id + "/" + file;
        else
            emote.url = "https://cdn.7tv.app/emote/" + id + (animated ? "/1x.gif" : "/1x.png");
        catalog[name] = emote;
    }
    return true;
}
