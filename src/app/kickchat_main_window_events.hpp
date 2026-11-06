#ifndef KICKCHAT_MAIN_WINDOW_EVENTS_HPP
#define KICKCHAT_MAIN_WINDOW_EVENTS_HPP

#include "kickchat_main_controls.hpp"
#include "kickchat_chat_selection.hpp"
#include "kickchat_channel_views.hpp"
#include "kickchat_preferences.hpp"

namespace mainWindowEvents {

enum { SidebarWidth = 145 };

struct ChatView {
    const MainControls &controls;
    const ChannelViews &channels;
    ChatSelectionController &selection;
    LinkHoverController &hover;
    const VisualPreferences &preferences;
    const std::deque<ChatLine> &lines;
    const std::wstring &status;
    const std::wstring &authStatus;
    HFONT font;
    HFONT bold;
    int &scroll;
    int &maxScroll;
    int &page;
};

bool handleChatEvent(HWND window, UINT message, WPARAM wp, LPARAM lp,
                     ChatView &view, LRESULT &result);

}

#endif
