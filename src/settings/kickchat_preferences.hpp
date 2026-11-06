#ifndef KICKCHAT_PREFERENCES_HPP
#define KICKCHAT_PREFERENCES_HPP

#include <string>
#include "kickchat_strings.hpp"

struct VisualPreferences {
    bool alwaysOnTop;
    bool linkPreviews;
    bool disableGifs;
    kickchatText::Language language;
    VisualPreferences() : alwaysOnTop(false), linkPreviews(false), disableGifs(false),
                          language(kickchatText::Spanish) {}
};

bool parseVisualPreferences(const std::string &data, VisualPreferences &settings);
std::string serializeVisualPreferences(const VisualPreferences &settings);
bool loadVisualPreferences(VisualPreferences &settings);
bool saveVisualPreferences(const VisualPreferences &settings);

#endif
