#include "kickchat_main_controls.hpp"
#include "kickchat_buttons.hpp"
#include "kickchat_strings.hpp"

static const int SIDEBAR_WIDTH = 145;

void layoutMainControls(HWND hwnd, const MainControls &controls) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0 || IsIconic(hwnd)) return;

    const int inputY = std::max(50, h - 96);
    const int scrollbarWidth = GetSystemMetrics(SM_CXVSCROLL);
    struct Placement { HWND control; int x, y, width, height; };
    const Placement placements[] = {
        {controls.addButton, std::max(SIDEBAR_WIDTH + 12, w - 259), 49, 132, 32},
        {controls.settingsButton, std::max(SIDEBAR_WIDTH + 148, w - 118), 49, 106, 32},
        {controls.messageEdit, SIDEBAR_WIDTH + 12, inputY,
            std::max(70, w - SIDEBAR_WIDTH - 117), 32},
        {controls.sendButton, std::max(SIDEBAR_WIDTH + 87, w - 95), inputY, 83, 32},
        {controls.chatScrollbar, w - scrollbarWidth, 95, scrollbarWidth, std::max(0, h - 203)}
    };
    const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    HDWP changes = BeginDeferWindowPos(5);
    if (changes) {
        for (size_t i = 0; i < sizeof(placements) / sizeof(placements[0]); ++i) {
            if (!placements[i].control) continue;
            changes = DeferWindowPos(changes, placements[i].control, NULL,
                                     placements[i].x, placements[i].y,
                                     placements[i].width, placements[i].height, flags);
            if (!changes) break;
        }
        if (changes && EndDeferWindowPos(changes)) return;
    }
    for (size_t i = 0; i < sizeof(placements) / sizeof(placements[0]); ++i) {
        if (!placements[i].control) continue;
        SetWindowPos(placements[i].control, NULL, placements[i].x, placements[i].y,
                     placements[i].width, placements[i].height, flags);
    }
}

void createMainControls(HWND hwnd, HINSTANCE instance, HFONT font,
                        WNDPROC editProc, MainControls &controls) {
    controls.addButton = CreateWindowExW(0, L"BUTTON", kickchatText::get(kickchatText::AddChannel),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | BS_OWNERDRAW,
        10, 10, 132, 32, hwnd, (HMENU)(INT_PTR)ID_ADD_CHANNEL, instance, NULL);
    controls.settingsButton = CreateWindowExW(0, L"BUTTON", kickchatText::get(kickchatText::SettingsButton),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | BS_OWNERDRAW,
        10, 10, 106, 32, hwnd, (HMENU)(INT_PTR)ID_SETTINGS, instance, NULL);
    controls.messageEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
        12, 100, 200, 32, hwnd, (HMENU)(INT_PTR)ID_EDIT, instance, NULL);
    controls.chatScrollbar = CreateWindowExW(0, L"SCROLLBAR", NULL,
        WS_CHILD | WS_CLIPSIBLINGS | SBS_VERT, 0, 0, 16, 100,
        hwnd, (HMENU)(INT_PTR)ID_CHAT_SCROLLBAR, instance, NULL);
    controls.sendButton = CreateWindowExW(0, L"BUTTON", kickchatText::get(kickchatText::SendButton),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | BS_OWNERDRAW,
        220, 100, 83, 32, hwnd, (HMENU)(INT_PTR)ID_SEND, instance, NULL);
    SendMessageW(controls.addButton, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(controls.sendButton, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(controls.settingsButton, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(controls.messageEdit, WM_SETFONT, (WPARAM)font, TRUE);
    SendMessageW(controls.messageEdit, EM_LIMITTEXT, 500, 0);
    controls.originalEditProc = (WNDPROC)SetWindowLongPtrW(controls.messageEdit, GWLP_WNDPROC, (LONG_PTR)editProc);
    layoutMainControls(hwnd, controls);
    EnableWindow(controls.sendButton, FALSE);
}
