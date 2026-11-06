#ifndef KICKCHAT_SCROLL_HPP
#define KICKCHAT_SCROLL_HPP

#include <algorithm>

inline int chatMaxScroll(int total, int page) {
    return std::max(0, total - page);
}

inline int chatScrollbarPosition(int maxScroll, int offsetFromBottom) {
    return maxScroll - std::max(0, std::min(maxScroll, offsetFromBottom));
}

inline int chatScrollFromPosition(int maxScroll, int position) {
    return maxScroll - std::max(0, std::min(maxScroll, position));
}

inline int chatWheelScroll(int currentScroll, int maxScroll, int wheelDelta) {
    return std::max(0, std::min(maxScroll, currentScroll + wheelDelta / 120 * 62));
}

#endif
