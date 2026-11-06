#include "kickchat_main_window_events.hpp"
#include "kickchat_chat_paint.hpp"
#include "kickchat_scroll.hpp"
#include "kickchat_emotes.hpp"
#include <windowsx.h>

namespace mainWindowEvents {
namespace {

void paint(HWND window, ChatView &view) {
    std::vector<std::string> channelNames;
    for (size_t i = 0; i < view.channels.channels.size(); ++i)
        channelNames.push_back(view.channels.channels[i].slug);

    paintChat(window, view.font, view.bold, view.status, view.authStatus,
              view.lines, view.scroll, channelNames, view.channels.active,
              view.maxScroll, GetSystemMetrics(SM_CXVSCROLL), view.selection.selection);
    view.hover.validate(window);

    RECT client = {};
    GetClientRect(window, &client);
    view.page = std::max(1L, client.bottom - 203);
    if (!view.controls.chatScrollbar) return;

    SCROLLINFO info = {};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = view.maxScroll + view.page - 1;
    info.nPage = static_cast<UINT>(view.page);
    info.nPos = chatScrollbarPosition(view.maxScroll, view.scroll);
    SetScrollInfo(view.controls.chatScrollbar, SB_CTL, &info, TRUE);
    ShowWindow(view.controls.chatScrollbar, view.maxScroll > 0 ? SW_SHOW : SW_HIDE);
}

void verticalScroll(HWND window, WPARAM wp, ChatView &view) {
    view.hover.clear(window);
    SCROLLINFO current = {};
    current.cbSize = sizeof(current);
    current.fMask = SIF_TRACKPOS | SIF_POS | SIF_PAGE | SIF_RANGE;
    GetScrollInfo(view.controls.chatScrollbar, SB_CTL, &current);
    int position = chatScrollbarPosition(view.maxScroll, view.scroll);
    switch (LOWORD(wp)) {
        case SB_LINEUP: position -= 38; break;
        case SB_LINEDOWN: position += 38; break;
        case SB_PAGEUP: position -= view.page; break;
        case SB_PAGEDOWN: position += view.page; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: position = current.nTrackPos; break;
        case SB_TOP: position = 0; break;
        case SB_BOTTOM: position = view.maxScroll; break;
        default: return;
    }
    view.scroll = chatScrollFromPosition(view.maxScroll, position);
    InvalidateRect(window, NULL, FALSE);
}

bool setChatCursor(HWND window, LPARAM lp) {
    if (LOWORD(lp) != HTCLIENT) return false;
    POINT point = {};
    ChatRecord user;
    if (!GetCursorPos(&point) || !ScreenToClient(window, &point)) return false;
    if (hasChatLinkAt(point.x, point.y) || chatUserAt(point.x, point.y, user)) {
        SetCursor(LoadCursor(NULL, IDC_HAND));
        return true;
    }
    ChatTextPosition caret;
    if (chatTextAt(point.x, point.y, caret)) {
        SetCursor(LoadCursor(NULL, IDC_IBEAM));
        return true;
    }
    return false;
}

void onTimer(HWND window, WPARAM timer, ChatView &view) {
    if (timer == LinkHoverController::Timer) {
        view.hover.begin(window, view.preferences.linkPreviews);
        return;
    }
    if (timer == ChatSelectionController::AutoScrollTimer) {
        view.selection.autoScroll(window, view.scroll, view.maxScroll);
        return;
    }
    if (timer != 1 || !IsWindowVisible(window) || !IsWindowEnabled(window) ||
        IsIconic(window) || !advanceAnimatedEmotes(GetTickCount())) return;

    RECT client = {};
    GetClientRect(window, &client);
    RECT messages = {SidebarWidth, 95, client.right, std::max(95L, client.bottom - 108)};
    if (messages.right > messages.left && messages.bottom > messages.top)
        InvalidateRect(window, &messages, FALSE);
}

} // namespace

bool handleChatEvent(HWND window, UINT message, WPARAM wp, LPARAM lp,
                     ChatView &view, LRESULT &result) {
    result = 0;
    switch (message) {
        case WM_MOUSEMOVE:
            if (view.selection.selecting)
                view.selection.mouseMove(window, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            else
                view.hover.track(window, GET_X_LPARAM(lp), GET_Y_LPARAM(lp),
                                 view.preferences.linkPreviews, view.selection.selecting);
            return true;
        case WM_MOUSELEAVE:
            view.hover.clear(window);
            return true;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE) view.hover.clear(window);
            result = DefWindowProcW(window, message, wp, lp);
            return true;
        case WM_LBUTTONUP:
            view.selection.mouseUp(window, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return true;
        case WM_CAPTURECHANGED:
            view.selection.cancel(window);
            return true;
        case WM_KEYDOWN:
            if (!(GetKeyState(VK_CONTROL) & 0x8000)) return false;
            if (wp == 'C') { view.selection.copySelected(window, view.lines); return true; }
            if (wp == 'A') { view.selection.selectAll(window, view.lines); return true; }
            if (wp == 'V' && view.controls.messageEdit) {
                SetFocus(view.controls.messageEdit);
                SendMessageW(view.controls.messageEdit, WM_PASTE, 0, 0);
                return true;
            }
            return false;
        case WM_MOUSEWHEEL:
            view.hover.clear(window);
            view.scroll = chatWheelScroll(view.scroll, view.maxScroll,
                                          GET_WHEEL_DELTA_WPARAM(wp));
            InvalidateRect(window, NULL, FALSE);
            return true;
        case WM_VSCROLL:
            if (reinterpret_cast<HWND>(lp) != view.controls.chatScrollbar) return false;
            verticalScroll(window, wp, view);
            return true;
        case WM_SETCURSOR:
            if (!setChatCursor(window, lp)) return false;
            result = TRUE;
            return true;
        case WM_SIZE:
            view.hover.clear(window);
            if (wp != SIZE_MINIMIZED) {
                layoutMainControls(window, view.controls);
                RedrawWindow(window, NULL, NULL,
                             RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
            }
            return true;
        case WM_PAINT:
            paint(window, view);
            return true;
        case WM_TIMER:
            onTimer(window, wp, view);
            return true;
        default:
            return false;
    }
}

} // namespace mainWindowEvents
