#ifndef KICKCHAT_EMOTES_HPP
#define KICKCHAT_EMOTES_HPP

#include "kickchat_emote_tokens.hpp"
#include <string>
#include "kickchat_platform.hpp"

namespace Gdiplus { class Bitmap; }

struct EmoteCatalogDownload {
    std::string channel;
    EmoteCatalog catalog;
};

struct EmoteDownload {
    std::string key;
    std::string bytes;
};

void startEmoteService(HWND window);
void stopEmoteService();
void requestSevenTvChannel(unsigned long long kickUserId, const std::string &slug);
void activateEmoteChannel(const std::string &slug);
void applyEmoteCatalog(EmoteCatalogDownload *download);
void applyEmoteDownload(EmoteDownload *download);
const EmoteCatalog &currentEmoteCatalog();
Gdiplus::Bitmap *cachedEmoteImage(const std::string &key);
void clearVisibleEmotes();
void markVisibleEmote(const std::string &key);
bool advanceAnimatedEmotes(DWORD now);
void setAnimatedEmotesEnabled(bool enabled);
Gdiplus::Bitmap *findEmoteImage(const std::string &key, const std::string &url);

#endif
