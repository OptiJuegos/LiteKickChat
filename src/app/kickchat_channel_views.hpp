#ifndef KICKCHAT_CHANNEL_VIEWS_HPP
#define KICKCHAT_CHANNEL_VIEWS_HPP

#include "kickchat_platform.hpp"
#include "kickchat_stream.hpp"
#include "kickchat_message_input.hpp"
#include "kickchat_text_selection.hpp"
#include "kickchat_link_hover.hpp"
#include "kickchat_strings.hpp"

struct ChannelView {
    std::string slug;
    std::wstring status;
    unsigned long long broadcasterId;
    int scroll;
    ChannelSession *session;
    std::wstring draft;
    std::deque<ChatLine> lines;
    ChannelView(const std::string &name) : slug(name), status(kickchatText::get(kickchatText::ConnectChannel)),
        broadcasterId(0), scroll(0), session(NULL) {}
};

struct ChannelUiState {
    std::deque<ChatLine> &lines;
    int &scroll;
    ChatTextSelection &selection;
    std::wstring &status;
    HWND messageEdit;
    HWND sendButton;
    LinkHoverController &hover;

    ChannelUiState(std::deque<ChatLine> &visibleLines, int &currentScroll,
                   ChatTextSelection &currentSelection, std::wstring &currentStatus,
                   HWND edit, HWND send, LinkHoverController &linkHover)
        : lines(visibleLines), scroll(currentScroll), selection(currentSelection),
          status(currentStatus), messageEdit(edit), sendButton(send), hover(linkHover) {}
};

struct ChannelViews {
    std::map<std::string, std::map<std::wstring, SeenUser> > seenUsers;
    std::vector<ChannelView> channels;
    std::vector<ChannelSession *> sessions;
    int active;
    unsigned manualChatId;

    ChannelViews() : active(-1), manualChatId(0) {}
    int index(const std::string &slug) const;
    void select(HWND window, int index, ChannelUiState &ui);
    bool add(HWND window, const std::string &slug, bool persist, ChannelUiState &ui);
    void remove(HWND window, int index, ChannelUiState &ui);
    void shutdown();

private:
    void saveChannelList() const;
};

#endif
