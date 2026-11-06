#include "kickchat_chat_selection.hpp"
#include "kickchat_chat_paint.hpp"
#include "kickchat_message_input.hpp"
#include <cstdlib>

void ChatSelectionController::mouseDown(HWND window, int x, int y) {
    ChatTextPosition position;
    if (chatTextAt(x, y, position)) {
        SetFocus(window);
        selection.active = true;
        selection.anchor = selection.focus = position;
        selecting = true;
        dragged = false;
        dragStart.x = x;
        dragStart.y = y;
        clickLink.clear();
        chatLinkUrlAt(x, y, clickLink);
        SetCapture(window);
    } else selection = ChatTextSelection();
    InvalidateRect(window, NULL, FALSE);
}

void ChatSelectionController::mouseMove(HWND window, int x, int y) {
    if (!selecting) return;
    if (!dragged &&
        (abs(x - dragStart.x) > 3 || abs(y - dragStart.y) > 3)) {
        dragged = true;
        SetTimer(window, AutoScrollTimer, 60, NULL);
    }
    ChatTextPosition position;
    if (dragged && chatTextAt(x, y, position, true) &&
        (selection.focus.row != position.row ||
         selection.focus.offset != position.offset)) {
        selection.focus = position;
        InvalidateRect(window, NULL, FALSE);
    }
}

void ChatSelectionController::mouseUp(HWND window, int x, int y) {
    if (!selecting) return;
    ChatTextPosition position;
    if (dragged && chatTextAt(x, y, position, true))
        selection.focus = position;
    const bool click = !dragged && !hasSelectedText(selection) && !clickLink.empty();
    std::wstring atRelease;
    const bool sameLink = click && chatLinkUrlAt(x, y, atRelease) &&
                          atRelease == clickLink;
    selecting = false;
    KillTimer(window, AutoScrollTimer);
    clickLink.clear();
    ReleaseCapture();
    if (sameLink) openChatLink(window, x, y);
    InvalidateRect(window, NULL, FALSE);
}

void ChatSelectionController::cancel(HWND window) {
    selecting = false;
    KillTimer(window, AutoScrollTimer);
    clickLink.clear();
}

void ChatSelectionController::selectAll(HWND window, const std::deque<ChatLine> &lines) {
    selection = ChatTextSelection();
    if (!lines.empty()) {
        selection.active = true;
        selection.anchor = ChatTextPosition(0, 0);
        selection.focus = ChatTextPosition(
            static_cast<int>(lines.size()) - 1,
            chatMessageText(lines.back()).size());
    }
    InvalidateRect(window, NULL, FALSE);
}

bool ChatSelectionController::copySelected(HWND window, const std::deque<ChatLine> &lines) const {
    if (!hasSelectedText(selection)) return false;
    std::deque<std::wstring> visible;
    for (size_t i = 0; i < lines.size(); ++i)
        visible.push_back(chatMessageText(lines[i]));
    const std::wstring selected = selectedChatText(visible, selection);
    return !selected.empty() && copyText(window, selected);
}

void ChatSelectionController::autoScroll(HWND window, int &scroll, int maxScroll) {
    if (!selecting || !dragged || IsIconic(window)) return;
    POINT point = {};
    RECT client = {};
    if (!GetCursorPos(&point) || !ScreenToClient(window, &point)) return;
    GetClientRect(window, &client);
    const int top = 95;
    const int bottom = std::max(95L, client.bottom - 108L);
    int delta = 0;
    if (point.y < top) delta = std::min(50, 12 + (top - static_cast<int>(point.y)) / 2);
    else if (point.y >= bottom) delta = -std::min(50, 12 + (static_cast<int>(point.y) - bottom) / 2);
    if (!delta) return;
    const int next = std::max(0, std::min(maxScroll, scroll + delta));
    if (next == scroll) return;
    scroll = next;
    RedrawWindow(window, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    ChatTextPosition focus;
    if (chatTextAt(point.x, point.y, focus, true)) {
        selection.focus = focus;
        InvalidateRect(window, NULL, FALSE);
    }
}
