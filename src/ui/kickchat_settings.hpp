#ifndef KICKCHAT_SETTINGS_HPP
#define KICKCHAT_SETTINGS_HPP

#include "kickchat_platform.hpp"
#include "kickchat_preferences.hpp"

bool showCredentialSettings(HWND owner, bool &changed, bool &authorize,
                            VisualPreferences &preferences);

#endif
