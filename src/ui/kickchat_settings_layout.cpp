#include "kickchat_settings_layout.hpp"
#include "kickchat_strings.hpp"

#include <cassert>

int settingsScaled(int value, int dpi) {
    return MulDiv(value, dpi, 96);
}

static HWND addControl(HWND parent, const wchar_t *type, const wchar_t *text,
                       DWORD style, DWORD extra, int x, int y, int width, int height,
                       int id, HFONT font, int dpi) {
    HWND control = CreateWindowExW(extra, type, text, WS_CHILD | WS_VISIBLE | style,
                                   settingsScaled(x, dpi), settingsScaled(y, dpi),
                                   settingsScaled(width, dpi), settingsScaled(height, dpi),
                                   parent, (HMENU)(INT_PTR)id,
                                   (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE), NULL);
    if (control) SendMessageW(control, WM_SETFONT, (WPARAM)font, TRUE);
    return control;
}

void createSettingsControls(HWND hwnd, SettingsState &state) {
    const int dpi = state.dpi;
    const kickchatText::Language language = state.preferences.language;
    using kickchatText::get;
    addControl(hwnd, L"BUTTON", get(kickchatText::AccountTab, language), WS_TABSTOP | BS_OWNERDRAW,
               0, 28, 17, 218, 38, ID_TAB_ACCOUNT, state.font, dpi);
    addControl(hwnd, L"BUTTON", get(kickchatText::VisualTab, language), WS_TABSTOP | BS_OWNERDRAW,
               0, 264, 17, 218, 38, ID_TAB_VISUAL, state.font, dpi);
    state.accountControls.push_back(addControl(hwnd, L"STATIC", get(kickchatText::KickAccount, language),
               0, 0, 28, 86, 454, 32, ID_TITLE, state.titleFont, dpi));
    state.accountControls.push_back(addControl(hwnd, L"STATIC", get(kickchatText::AccountSubtitle, language),
               0, 0, 28, 121, 454, 24, ID_SUBTITLE, state.font, dpi));
    state.accountControls.push_back(addControl(hwnd, L"STATIC", L"Client ID",
               0, 0, 28, 156, 454, 22, 0, state.font, dpi));
    state.idEdit = addControl(hwnd, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL,
                             WS_EX_CLIENTEDGE, 28, 181, 454, 34,
                             ID_CLIENT_ID, state.font, dpi);
    state.accountControls.push_back(state.idEdit);
    state.accountControls.push_back(addControl(hwnd, L"STATIC", L"Client Secret",
               0, 0, 28, 235, 454, 22, 0, state.font, dpi));
    state.secretEdit = addControl(hwnd, L"EDIT", L"",
                                 WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL,
                                 WS_EX_CLIENTEDGE, 28, 260, 454, 34,
                                 ID_CLIENT_SECRET, state.font, dpi);
    state.accountControls.push_back(state.secretEdit);
    state.accountControls.push_back(addControl(hwnd, L"BUTTON", get(kickchatText::DevelopersButton, language),
               WS_TABSTOP | BS_OWNERDRAW, 0, 28, 314, 220, 36,
               ID_DEVELOPERS, state.font, dpi));
    state.authButton = addControl(hwnd, L"BUTTON", get(kickchatText::ConnectAccount, language),
               WS_TABSTOP | BS_OWNERDRAW, 0, 260, 314, 222, 36,
               ID_AUTHORIZE, state.font, dpi);
    state.accountControls.push_back(state.authButton);
    state.accountControls.push_back(addControl(hwnd, L"STATIC",
               get(kickchatText::DpapiHint, language), 0, 0,
               28, 363, 454, 24, ID_HINT, state.font, dpi));

    state.visualControls.push_back(addControl(hwnd, L"STATIC", get(kickchatText::Appearance, language),
               0, 0, 28, 86, 454, 32, 0, state.titleFont, dpi));
    state.visualControls.push_back(addControl(hwnd, L"STATIC",
               get(kickchatText::AppearanceSubtitle, language),
               0, 0, 28, 121, 454, 24, 0, state.font, dpi));
    state.visualControls.push_back(addControl(hwnd, L"STATIC",
               get(kickchatText::LanguageLabel, language), 0, 0,
               298, 90, 85, 27, 0, state.font, dpi));
    state.languageCombo = addControl(hwnd, L"COMBOBOX", L"",
               WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
               0, 384, 87, 99, 106, ID_LANGUAGE, state.font, dpi);
    state.visualControls.push_back(state.languageCombo);
    SendMessageW(state.languageCombo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"Español"));
    SendMessageW(state.languageCombo, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(state.languageCombo, CB_SETCURSEL,
                 state.preferences.language == kickchatText::English ? 1 : 0, 0);
    state.alwaysOnTopCheck = addControl(hwnd, L"BUTTON", get(kickchatText::AlwaysOnTop, language),
               WS_TABSTOP | BS_AUTOCHECKBOX, 0, 28, 163, 454, 31,
               ID_ALWAYS_TOP, state.font, dpi);
    state.visualControls.push_back(state.alwaysOnTopCheck);
    state.visualControls.push_back(addControl(hwnd, L"STATIC",
               get(kickchatText::AlwaysOnTopHint, language),
               0, 0, 51, 198, 430, 24, 0, state.font, dpi));
    state.linkPreviewsCheck = addControl(hwnd, L"BUTTON", get(kickchatText::Previews, language),
               WS_TABSTOP | BS_AUTOCHECKBOX, 0, 28, 245, 454, 31,
               ID_LINK_PREVIEWS, state.font, dpi);
    state.visualControls.push_back(state.linkPreviewsCheck);
    state.visualControls.push_back(addControl(hwnd, L"STATIC",
               get(kickchatText::PreviewsHint, language),
               0, 0, 51, 280, 430, 24, 0, state.font, dpi));
    state.visualControls.push_back(addControl(hwnd, L"STATIC",
               get(kickchatText::PreviewsNetworkHint, language),
               0, 0, 51, 307, 430, 24, ID_HINT, state.font, dpi));
    state.disableGifsCheck = addControl(hwnd, L"BUTTON", get(kickchatText::DisableGifs, language),
               WS_TABSTOP | BS_AUTOCHECKBOX, 0, 28, 341, 454, 29,
               ID_DISABLE_GIFS, state.font, dpi);
    state.visualControls.push_back(state.disableGifsCheck);
    state.visualControls.push_back(addControl(hwnd, L"STATIC",
               get(kickchatText::DisableGifsHint, language),
               0, 0, 51, 373, 430, 20, 0, state.font, dpi));
    SendMessageW(state.alwaysOnTopCheck, BM_SETCHECK,
                 state.preferences.alwaysOnTop ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.linkPreviewsCheck, BM_SETCHECK,
                 state.preferences.linkPreviews ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(state.disableGifsCheck, BM_SETCHECK,
                 state.preferences.disableGifs ? BST_CHECKED : BST_UNCHECKED, 0);
    state.saveButton = addControl(hwnd, L"BUTTON", get(kickchatText::Save, language),
               WS_TABSTOP | BS_OWNERDRAW, 0, 358, 411, 124, 38,
               ID_SAVE, state.font, dpi);
    addControl(hwnd, L"BUTTON", get(kickchatText::Cancel, language), WS_TABSTOP | BS_OWNERDRAW,
               0, 238, 411, 108, 38, ID_CANCEL, state.font, dpi);
    assert(181 + 34 < 235 && 260 + 34 < 314 && 373 + 20 < 398 &&
           411 + 38 < 465);
}
