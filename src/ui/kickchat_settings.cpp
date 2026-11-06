#include "kickchat_settings.hpp"
#include "kickchat_buttons.hpp"
#include "kickchat_credentials.hpp"
#include "kickchat_oauth.hpp"
#include "kickchat_preferences.hpp"
#include "kickchat_settings_layout.hpp"
#include "kickchat_strings.hpp"

static const wchar_t *SETTINGS_CLASS = L"KickChatCredentialSettings";

static const COLORREF PANEL_COLOR = RGB(25, 28, 32);
static const COLORREF FIELD_COLOR = RGB(37, 42, 48);

static void paintSettings(HWND hwnd, HDC dc, const SettingsState &state) {
    RECT client;
    GetClientRect(hwnd, &client);
    FillRect(dc, &client, state.background);
    HPEN line = CreatePen(PS_SOLID, 1, RGB(56, 62, 69));
    HGDIOBJ previous = SelectObject(dc, line);
    MoveToEx(dc, settingsScaled(28, state.dpi), settingsScaled(70, state.dpi), NULL);
    LineTo(dc, settingsScaled(482, state.dpi), settingsScaled(70, state.dpi));
    MoveToEx(dc, settingsScaled(28, state.dpi), settingsScaled(398, state.dpi), NULL);
    LineTo(dc, settingsScaled(482, state.dpi), settingsScaled(398, state.dpi));
    SelectObject(dc, previous);
    DeleteObject(line);
}

static void showSettingsPage(HWND dialog, SettingsState &state, int page) {
    state.page = page;
    for (size_t i = 0; i < state.accountControls.size(); ++i)
        ShowWindow(state.accountControls[i], page == 0 ? SW_SHOW : SW_HIDE);
    for (size_t i = 0; i < state.visualControls.size(); ++i)
        ShowWindow(state.visualControls[i], page == 1 ? SW_SHOW : SW_HIDE);
    HWND accountTab = GetDlgItem(dialog, ID_TAB_ACCOUNT);
    HWND visualTab = GetDlgItem(dialog, ID_TAB_VISUAL);
    if (accountTab) InvalidateRect(accountTab, NULL, TRUE);
    if (visualTab) InvalidateRect(visualTab, NULL, TRUE);
    InvalidateRect(dialog, NULL, FALSE);
}

static void updateAuthorizationButton(SettingsState &state) {
    if (!state.authButton) return;
    const LONG pending = InterlockedCompareExchange(&g_oauthPending, 0, 0);
    const bool cancelling = InterlockedCompareExchange(&g_oauthCancelRequested, 0, 0) != 0;
    const bool busy = InterlockedCompareExchange(&g_taskBusy, 0, 0) != 0;
    const bool authorized = InterlockedCompareExchange(&g_authenticated, 0, 0) != 0;
    const wchar_t *caption = pending == 1 ?
        (cancelling ? kickchatText::get(kickchatText::Cancelling) :
                      kickchatText::get(kickchatText::CancelAuthorization)) :
        pending == 2 ? kickchatText::get(kickchatText::Connecting) :
        authorized ? kickchatText::get(kickchatText::ReconnectAccount) :
                     kickchatText::get(kickchatText::ConnectAccount);
    wchar_t previous[64] = {0};
    GetWindowTextW(state.authButton, previous, 64);
    if (wcscmp(previous, caption) != 0) SetWindowTextW(state.authButton, caption);
    EnableWindow(state.authButton, !busy || (pending == 1 && !cancelling));
    EnableWindow(state.idEdit, !busy);
    EnableWindow(state.secretEdit, !busy);
    EnableWindow(state.saveButton, !busy);
}

static bool saveAppearance(HWND hwnd, SettingsState &state) {
    VisualPreferences selected = state.preferences;
    selected.alwaysOnTop = SendMessageW(state.alwaysOnTopCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    selected.linkPreviews = SendMessageW(state.linkPreviewsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    selected.disableGifs = SendMessageW(state.disableGifsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    selected.language = SendMessageW(state.languageCombo, CB_GETCURSEL, 0, 0) == 1 ?
        kickchatText::English : kickchatText::Spanish;
    if ((selected.alwaysOnTop != state.preferences.alwaysOnTop ||
         selected.linkPreviews != state.preferences.linkPreviews ||
         selected.disableGifs != state.preferences.disableGifs ||
         selected.language != state.preferences.language) &&
        !saveVisualPreferences(selected)) {
        MessageBoxW(hwnd, kickchatText::get(kickchatText::SaveAppearanceFailed),
                    L"KickChat", MB_ICONERROR | MB_OK);
        return false;
    }
    state.preferences = selected;
    kickchatText::setLanguage(selected.language);
    return true;
}

static void saveSettings(HWND hwnd, SettingsState &state) {
    if (state.page == 1) {
        if (!saveAppearance(hwnd, state)) return;
        state.saved = true;
        DestroyWindow(hwnd);
        return;
    }
    wchar_t idBuffer[202] = {0};
    wchar_t secretBuffer[302] = {0};
    GetWindowTextW(state.idEdit, idBuffer, 202);
    GetWindowTextW(state.secretEdit, secretBuffer, 302);
    std::string id = utf8FromWide(idBuffer);
    std::string secret = utf8FromWide(secretBuffer);
    std::wstring error;
    if (!saveAppearance(hwnd, state)) {
        SecureZeroMemory(secretBuffer, sizeof(secretBuffer));
        if (!secret.empty()) SecureZeroMemory(&secret[0], secret.size());
        return;
    }
    bool changed = false;
    bool saved = saveCredentials(id, secret, changed, error);
    SecureZeroMemory(secretBuffer, sizeof(secretBuffer));
    if (!secret.empty()) SecureZeroMemory(&secret[0], secret.size());
    if (!saved) {
        MessageBoxW(hwnd, error.c_str(), kickchatText::get(kickchatText::SaveFailed), MB_ICONERROR | MB_OK);
        return;
    }
    state.saved = true;
    state.changed = changed;
    DestroyWindow(hwnd);
}

static LRESULT CALLBACK settingsProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    SettingsState *state = (SettingsState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (message) {
        case WM_NCCREATE: {
            CREATESTRUCTW *create = (CREATESTRUCTW *)lp;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
            return TRUE;
        }
        case WM_CREATE: {
            state = (SettingsState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            state->font = CreateFontW(-settingsScaled(16, state->dpi), 0, 0, 0, FW_NORMAL,
                                      FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            state->titleFont = CreateFontW(-settingsScaled(23, state->dpi), 0, 0, 0, FW_SEMIBOLD,
                                           FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            state->background = CreateSolidBrush(PANEL_COLOR);
            state->fieldBackground = CreateSolidBrush(FIELD_COLOR);
            createSettingsControls(hwnd, *state);
            showSettingsPage(hwnd, *state, 0);
            if (!state->idEdit || !state->secretEdit || !state->authButton ||
                !state->saveButton || !state->disableGifsCheck) return -1;
            SendMessageW(state->idEdit, EM_LIMITTEXT, 200, 0);
            SendMessageW(state->secretEdit, EM_LIMITTEXT, 300, 0);
            std::string id, secret;
            if (loadCredentials(id, secret)) {
                SetWindowTextW(state->idEdit, toWide(id).c_str());
                SetWindowTextW(state->secretEdit, toWide(secret).c_str());
                if (!secret.empty()) SecureZeroMemory(&secret[0], secret.size());
            }
            updateAuthorizationButton(*state);
            SetTimer(hwnd, 1, 200, NULL);
            return 0;
        }
        case WM_TIMER:
            if (state && wp == 1) updateAuthorizationButton(*state);
            return 0;
        case WM_ERASEBKGND: {
            if (!state || !state->background) break;
            RECT client;
            GetClientRect(hwnd, &client);
            FillRect((HDC)wp, &client, state->background);
            return TRUE;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            if (state && state->background) paintSettings(hwnd, dc, *state);
            EndPaint(hwnd, &paint);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            if (!state || !state->background) break;
            HDC dc = (HDC)wp;
            int id = GetDlgCtrlID((HWND)lp);
            COLORREF foreground = RGB(234, 238, 243);
            if (id == ID_TITLE) foreground = RGB(91, 226, 141);
            if (id == ID_SUBTITLE || id == ID_HINT) foreground = RGB(166, 176, 187);
            SetTextColor(dc, foreground);
            SetBkColor(dc, PANEL_COLOR);
            return (LRESULT)state->background;
        }
        case WM_CTLCOLOREDIT: {
            if (!state || !state->fieldBackground) break;
            HDC dc = (HDC)wp;
            SetTextColor(dc, RGB(239, 243, 245));
            SetBkColor(dc, FIELD_COLOR);
            return (LRESULT)state->fieldBackground;
        }
        case WM_DRAWITEM: {
            const DRAWITEMSTRUCT *item = (const DRAWITEMSTRUCT *)lp;
            if (state && item && (item->CtlID == ID_SAVE || item->CtlID == ID_CANCEL ||
                                  item->CtlID == ID_DEVELOPERS || item->CtlID == ID_AUTHORIZE ||
                                  item->CtlID == ID_TAB_ACCOUNT || item->CtlID == ID_TAB_VISUAL))
                return drawRoundedButton(item, state->font, PANEL_COLOR,
                                         item->CtlID == ID_SAVE ||
                                         (item->CtlID == ID_TAB_ACCOUNT && state->page == 0) ||
                                         (item->CtlID == ID_TAB_VISUAL && state->page == 1)) ? TRUE : FALSE;
            break;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if ((id == ID_TAB_ACCOUNT || id == ID_TAB_VISUAL) && HIWORD(wp) == BN_CLICKED) {
                showSettingsPage(hwnd, *state, id == ID_TAB_VISUAL ? 1 : 0);
                return 0;
            }
            if (id == ID_AUTHORIZE && HIWORD(wp) == BN_CLICKED) {
                if (InterlockedCompareExchange(&g_oauthPending, 0, 0) == 1) {
                    InterlockedExchange(&g_oauthCancelRequested, 1);
                    updateAuthorizationButton(*state);
                } else if (InterlockedCompareExchange(&g_taskBusy, 0, 0) == 0) {
                    state->authorize = true;
                    saveSettings(hwnd, *state);
                    if (!state->saved) state->authorize = false;
                }
                return 0;
            }
            if (id == ID_SAVE && HIWORD(wp) == BN_CLICKED) {
                saveSettings(hwnd, *state);
                return 0;
            }
            if (id == ID_CANCEL && HIWORD(wp) == BN_CLICKED) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == ID_DEVELOPERS && HIWORD(wp) == BN_CLICKED) {
                HINSTANCE opened = ShellExecuteW(hwnd, L"open",
                    L"https://kick.com/settings/developer", NULL, NULL, SW_SHOWNORMAL);
                if ((INT_PTR)opened <= 32)
                    MessageBoxW(hwnd, kickchatText::get(kickchatText::DevelopersOpenFailed), L"KickChat", MB_ICONERROR);
                return 0;
            }
            break;
        }
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            if (state) {
                if (state->secretEdit) SetWindowTextW(state->secretEdit, L"");
                DeleteObject(state->font);
                DeleteObject(state->titleFont);
                DeleteObject(state->fieldBackground);
                DeleteObject(state->background);
            }
            return 0;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

bool showCredentialSettings(HWND owner, bool &changed, bool &authorize,
                            VisualPreferences &preferences) {
    changed = false;
    authorize = false;
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
    WNDCLASSEXW cls = {0};
    cls.cbSize = sizeof(cls);
    cls.hInstance = instance;
    cls.lpszClassName = SETTINGS_CLASS;
    cls.lpfnWndProc = settingsProc;
    cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&cls);

    HDC dc = GetDC(owner);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(owner, dc);
    RECT desired = {0, 0, settingsScaled(510, dpi), settingsScaled(465, dpi)};
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    DWORD extra = WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT;
    AdjustWindowRectEx(&desired, style, FALSE, extra);
    int width = desired.right - desired.left;
    int height = desired.bottom - desired.top;
    RECT parent;
    GetWindowRect(owner, &parent);
    int x = parent.left + ((parent.right - parent.left) - width) / 2;
    int y = parent.top + ((parent.bottom - parent.top) - height) / 2;
    MONITORINFO monitor = {sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor)) {
        x = std::max<LONG>(monitor.rcWork.left, std::min<LONG>(x, monitor.rcWork.right - width));
        y = std::max<LONG>(monitor.rcWork.top, std::min<LONG>(y, monitor.rcWork.bottom - height));
    }
    SettingsState state;
    state.dpi = dpi;
    state.preferences = preferences;
    HWND dialog = CreateWindowExW(extra, SETTINGS_CLASS,
                                   kickchatText::get(kickchatText::SettingsTitle, preferences.language),
                                   style, x, y, width, height,
                                   owner, NULL, instance, &state);
    if (!dialog) return false;
    EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);
    SetFocus(state.idEdit);
    MSG msg;
    while (IsWindow(dialog)) {
        BOOL result = GetMessageW(&msg, NULL, 0, 0);
        if (result <= 0) {
            if (result == 0) PostQuitMessage((int)msg.wParam);
            break;
        }
        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    changed = state.changed;
    authorize = state.saved && state.authorize;
    if (state.saved) preferences = state.preferences;
    return state.saved;
}
