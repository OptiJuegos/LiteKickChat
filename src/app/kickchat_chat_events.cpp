#include "kickchat_chat_events.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_history.hpp"
#include "kickchat_profile.hpp"
#include "kickchat_emotes.hpp"
#include "kickchat_oauth.hpp"

static const size_t MAX_MESSAGES = 250;

void receiveChatPacket(const UiPacket &packet, ChannelViews &views,
                       std::deque<ChatLine> &visible, ChatTextSelection &selection, int &scroll) {
    if ((packet.chat.channel.empty() || views.index(packet.chat.channel) >= 0) &&
        packet.chat.userId && !packet.chat.user.empty()) {
        std::map<std::wstring, SeenUser> &known = views.seenUsers[packet.chat.channel];
        if (known.size() >= 512) known.clear();
        SeenUser person = {packet.chat.user, packet.chat.userId};
        known[kickcmd::lower(person.name)] = person;
        appendChatHistory(packet.chat.record);
        refreshUserProfile(packet.chat.record);
    }
    int index = views.index(packet.chat.channel);
    std::deque<ChatLine> *lines = NULL;
    if (packet.chat.channel.empty()) lines = &visible;
    else if (index == views.active) lines = &visible;
    else if (index >= 0) lines = &views.channels[index].lines;
    bool duplicate = false;
    if (lines && !packet.chat.record.messageId.empty()) {
        for (std::deque<ChatLine>::const_iterator it = lines->begin();
             it != lines->end(); ++it) {
            if (it->record.messageId == packet.chat.record.messageId) {
                duplicate = true;
                break;
            }
        }
    }
    if (lines && !duplicate) {
        if (index == views.active && scroll > 0) scroll += 44;
        if (packet.chat.backfill) {
            std::deque<ChatLine>::iterator at = lines->begin();
            while (at != lines->end() &&
                   at->record.receivedAt <= packet.chat.record.receivedAt) ++at;
            if (lines == &visible && selection.active) {
                const int insertion = static_cast<int>(at - lines->begin());
                if (selection.anchor.row >= insertion) ++selection.anchor.row;
                if (selection.focus.row >= insertion) ++selection.focus.row;
            }
            lines->insert(at, packet.chat);
        } else lines->push_back(packet.chat);
        if (lines->size() > MAX_MESSAGES) {
            lines->pop_front();
            if (lines == &visible && selection.active) {
                if (selection.anchor.row == 0 || selection.focus.row == 0)
                    selection = ChatTextSelection();
                else { --selection.anchor.row; --selection.focus.row; }
            }
        }
    }
}

void receiveChannelIdPacket(const UiPacket &packet, ChannelViews &views, HWND sendButton) {
    int index = views.index(packet.channel);
    if (index >= 0) {
        views.channels[index].broadcasterId = packet.broadcasterId;
        requestSevenTvChannel(packet.broadcasterId, packet.channel);
        if (index == views.active && InterlockedCompareExchange(&g_authenticated, 0, 0) &&
            !InterlockedCompareExchange(&g_taskBusy, 0, 0)) EnableWindow(sendButton, TRUE);
    }
}

void receiveStatusPacket(const UiPacket &packet, ChannelViews &views, std::wstring &status) {
    int index = views.index(packet.channel);
    if (index >= 0) views.channels[index].status = packet.status;
    if (index == views.active || packet.channel.empty()) status = packet.status;
}

void receiveSendResult(const SendResult &result, ChannelViews &views,
                       std::wstring &authStatus, HWND messageEdit) {
    completeUserProfileAction(result);
    authStatus=result.message;
    if(result.original.size() && result.original[0]==L'/')
        addChat(utf8FromWide(kickchatText::get(kickchatText::CommandsLabel)),
                utf8FromWide(result.message),
                result.success ? RGB(128,201,255) : RGB(255,140,140), 0, result.channel);
    if (result.success && views.index(result.channel) >= 0 && result.channel !=
        (views.active >= 0 ? views.channels[views.active].slug : "") &&
        views.channels[views.index(result.channel)].draft == result.original)
        views.channels[views.index(result.channel)].draft.clear();
    if(result.success && messageEdit && views.active >= 0 &&
       views.channels[views.active].slug == result.channel){
        int len=GetWindowTextLengthW(messageEdit);
        std::wstring current(len+1,L'\0');GetWindowTextW(messageEdit,&current[0],len+1);
        current.resize(wcslen(current.c_str()));
        if(current==result.original)SetWindowTextW(messageEdit,L"");
    }
}
