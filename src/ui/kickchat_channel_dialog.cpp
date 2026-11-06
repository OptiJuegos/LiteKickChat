#include "kickchat_channel_dialog.hpp"
#include "kickchat_channels.hpp"
#include "kickchat_buttons.hpp"
#include "kickchat_strings.hpp"

static const int ID_CHANNEL_EDIT = 4101;
static const int ID_CHANNEL_ADD = 4102;
static const int ID_CHANNEL_CANCEL = 4103;
static const COLORREF BACKGROUND = RGB(25, 28, 32);
static const COLORREF FIELD = RGB(37, 42, 48);

struct ChannelDialogState {
    HWND edit;
    HFONT font;
    HBRUSH background;
    HBRUSH field;
    int dpi;
    std::string slug;
    bool accepted;
    ChannelDialogState() : edit(NULL), font(NULL), background(NULL), field(NULL),
                           dpi(96), accepted(false) {}
};

static int scale(int value, int dpi) { return MulDiv(value, dpi, 96); }

static LRESULT CALLBACK channelDialogProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    ChannelDialogState *state = reinterpret_cast<ChannelDialogState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lp);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    if (msg == WM_CREATE && state) {
        HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));
        state->font = CreateFontW(-scale(16, state->dpi), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        state->background = CreateSolidBrush(BACKGROUND);
        state->field = CreateSolidBrush(FIELD);
        HWND label = CreateWindowExW(0, L"STATIC", kickchatText::get(kickchatText::ChannelDialogLabel),
            WS_CHILD | WS_VISIBLE, scale(20, state->dpi), scale(17, state->dpi),
            scale(300, state->dpi), scale(23, state->dpi), window, NULL, instance, NULL);
        state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            scale(20, state->dpi), scale(48, state->dpi), scale(310, state->dpi),
            scale(31, state->dpi), window, (HMENU)(INT_PTR)ID_CHANNEL_EDIT, instance, NULL);
        HWND add = CreateWindowExW(0, L"BUTTON", kickchatText::get(kickchatText::ChannelDialogAdd),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            scale(152, state->dpi), scale(97, state->dpi), scale(85, state->dpi),
            scale(33, state->dpi), window, (HMENU)(INT_PTR)ID_CHANNEL_ADD, instance, NULL);
        HWND cancel = CreateWindowExW(0, L"BUTTON", kickchatText::get(kickchatText::Cancel),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            scale(245, state->dpi), scale(97, state->dpi), scale(85, state->dpi),
            scale(33, state->dpi), window, (HMENU)(INT_PTR)ID_CHANNEL_CANCEL, instance, NULL);
        HWND controls[] = {label, state->edit, add, cancel};
        for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i)
            if (controls[i]) SendMessageW(controls[i], WM_SETFONT, (WPARAM)state->font, TRUE);
        SendMessageW(state->edit, EM_LIMITTEXT, 64, 0);
        SetFocus(state->edit);
        return 0;
    }
    if (msg == WM_COMMAND && state) {
        if (LOWORD(wp) == ID_CHANNEL_ADD) {
            wchar_t value[128] = {0};
            GetWindowTextW(state->edit, value, 128);
            state->slug = normalizeChannel(utf8FromWide(value));
            if (state->slug.empty()) {
                MessageBoxW(window, kickchatText::get(kickchatText::ChannelDialogInvalid),
                            kickchatText::get(kickchatText::ChannelDialogInvalidTitle),
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            state->accepted = true;
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wp) == ID_CHANNEL_CANCEL) { DestroyWindow(window); return 0; }
    }
    if (msg == WM_CTLCOLORSTATIC && state) {
        HDC dc = (HDC)wp;
        SetBkColor(dc, BACKGROUND);
        SetTextColor(dc, RGB(232, 238, 241));
        return (LRESULT)state->background;
    }
    if (msg == WM_CTLCOLOREDIT && state) {
        HDC dc = (HDC)wp;
        SetBkColor(dc, FIELD);
        SetTextColor(dc, RGB(239, 244, 246));
        return (LRESULT)state->field;
    }
    if (msg == WM_DRAWITEM && state) {
        const DRAWITEMSTRUCT *item = reinterpret_cast<const DRAWITEMSTRUCT *>(lp);
        return drawRoundedButton(item, state->font, BACKGROUND, item->CtlID == ID_CHANNEL_ADD);
    }
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && state) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(window, &ps);
        RECT area; GetClientRect(window, &area);
        FillRect(dc, &area, state->background);
        EndPaint(window, &ps);
        return 0;
    }
    if (msg == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (msg == WM_DESTROY && state) {
        DeleteObject(state->font);
        DeleteObject(state->background);
        DeleteObject(state->field);
        return 0;
    }
    return DefWindowProcW(window, msg, wp, lp);
}

bool promptChannel(HWND owner, std::string &slug) {
    static const wchar_t *name = L"KickChatChannelDialog";
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
    WNDCLASSW cls = {0};
    cls.lpfnWndProc = channelDialogProc;
    cls.hInstance = instance;
    cls.lpszClassName = name;
    RegisterClassW(&cls);
    ChannelDialogState state;
    HDC dc = GetDC(owner);
    if (dc) { state.dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(owner, dc); }
    RECT parent; GetWindowRect(owner, &parent);
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, name,
        kickchatText::get(kickchatText::ChannelDialogTitle),
        WS_CAPTION | WS_SYSMENU | WS_POPUP, parent.left + 45, parent.top + 65,
        scale(352, state.dpi), scale(177, state.dpi), owner, NULL, instance, &state);
    if (!window) return false;
    EnableWindow(owner, FALSE);
    ShowWindow(window, SW_SHOW);
    MSG msg;
    int result = 1;
    while (IsWindow(window) && (result = GetMessageW(&msg, NULL, 0, 0)) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            DestroyWindow(window);
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(ID_CHANNEL_ADD, BN_CLICKED), 0);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (result == 0) PostQuitMessage((int)msg.wParam);
    if (state.accepted) slug = state.slug;
    return state.accepted;
}
