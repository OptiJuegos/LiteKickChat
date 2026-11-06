#ifndef KICKCHAT_TEXT_SELECTION_HPP
#define KICKCHAT_TEXT_SELECTION_HPP

#include <string>
#include <deque>
#include <algorithm>

struct ChatTextPosition {
    int row;
    size_t offset;
    ChatTextPosition(int r = -1, size_t o = 0) : row(r), offset(o) {}
};

struct ChatTextSelection {
    ChatTextPosition anchor, focus;
    bool active;
    ChatTextSelection() : active(false) {}
};

inline bool hasSelectedText(const ChatTextSelection &selection) {
    return selection.active && selection.anchor.row >= 0 && selection.focus.row >= 0 &&
           (selection.anchor.row != selection.focus.row ||
            selection.anchor.offset != selection.focus.offset);
}

inline std::wstring selectedChatText(const std::deque<std::wstring> &displayTexts,
                                     const ChatTextSelection &selection) {
    if (!hasSelectedText(selection) || displayTexts.empty()) return L"";

    ChatTextPosition start = selection.anchor;
    ChatTextPosition end = selection.focus;
    const int lastRow = static_cast<int>(displayTexts.size() - 1);
    if (start.row > lastRow) {
        start.row = lastRow;
        start.offset = displayTexts.back().size();
    } else {
        start.offset = std::min(start.offset, displayTexts[start.row].size());
    }
    if (end.row > lastRow) {
        end.row = lastRow;
        end.offset = displayTexts.back().size();
    } else {
        end.offset = std::min(end.offset, displayTexts[end.row].size());
    }

    if (start.row > end.row || (start.row == end.row && start.offset > end.offset))
        std::swap(start, end);
    if (start.row == end.row)
        return displayTexts[start.row].substr(start.offset, end.offset - start.offset);

    std::wstring selected = displayTexts[start.row].substr(start.offset);
    for (int row = start.row + 1; row < end.row; ++row) {
        selected += L"\r\n";
        selected += displayTexts[row];
    }
    selected += L"\r\n";
    selected += displayTexts[end.row].substr(0, end.offset);
    return selected;
}

#endif
