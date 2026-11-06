#ifndef KICKCHAT_SETTINGS_LAYOUT_HPP
#define KICKCHAT_SETTINGS_LAYOUT_HPP

#include "kickchat_platform.hpp"
#include "kickchat_preferences.hpp"

static const int ID_CLIENT_ID = 4101;
static const int ID_CLIENT_SECRET = 4102;
static const int ID_TITLE = 4105;
static const int ID_SUBTITLE = 4106;
static const int ID_HINT = 4107;
static const int ID_DEVELOPERS = 4108;
static const int ID_AUTHORIZE = 4109;
static const int ID_TAB_ACCOUNT = 4110;
static const int ID_TAB_VISUAL = 4111;
static const int ID_ALWAYS_TOP = 4112;
static const int ID_LINK_PREVIEWS = 4113;
static const int ID_DISABLE_GIFS = 4114;
static const int ID_LANGUAGE = 4115;
static const int ID_SAVE = IDOK;
static const int ID_CANCEL = IDCANCEL;

struct SettingsState {
    HWND idEdit;
    HWND secretEdit;
    HWND authButton;
    HWND saveButton;
    HWND alwaysOnTopCheck;
    HWND linkPreviewsCheck;
    HWND disableGifsCheck;
    HWND languageCombo;
    std::vector<HWND> accountControls;
    std::vector<HWND> visualControls;
    VisualPreferences preferences;
    int page;
    HFONT font;
    HFONT titleFont;
    HBRUSH background;
    HBRUSH fieldBackground;
    int dpi;
    bool saved;
    bool changed;
    bool authorize;
    SettingsState() : idEdit(NULL), secretEdit(NULL), authButton(NULL), saveButton(NULL), alwaysOnTopCheck(NULL),
                      linkPreviewsCheck(NULL), disableGifsCheck(NULL), languageCombo(NULL), page(0),
                      font(NULL), titleFont(NULL),
                      background(NULL), fieldBackground(NULL), dpi(96),
                      saved(false), changed(false), authorize(false) {}
};

int settingsScaled(int value, int dpi);
void createSettingsControls(HWND dialog, SettingsState &state);

#endif
