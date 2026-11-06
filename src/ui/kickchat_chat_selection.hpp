#ifndef KICKCHAT_CHAT_SELECTION_HPP
#define KICKCHAT_CHAT_SELECTION_HPP

#include "kickchat_platform.hpp"
#include "kickchat_text_selection.hpp"

struct ChatSelectionController {
    enum { AutoScrollTimer = 3 };

    ChatTextSelection selection;
    bool selecting;
    bool dragged;
    POINT dragStart;
    std::wstring clickLink;

    ChatSelectionController() : selecting(false), dragged(false) {
        dragStart.x = dragStart.y = 0;
    }

    void mouseDown(HWND window, int x, int y);
    void mouseMove(HWND window, int x, int y);
    void mouseUp(HWND window, int x, int y);
    void cancel(HWND window);
    void selectAll(HWND window, const std::deque<ChatLine> &lines);
    bool copySelected(HWND window, const std::deque<ChatLine> &lines) const;
    void autoScroll(HWND window, int &scroll, int maxScroll);
};

#endif
