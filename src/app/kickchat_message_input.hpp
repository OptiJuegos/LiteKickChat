#ifndef KICKCHAT_MESSAGE_INPUT_HPP
#define KICKCHAT_MESSAGE_INPUT_HPP

#include "kickchat_platform.hpp"
#include <map>

struct SeenUser {
    std::wstring name;
    unsigned long long id;
};

struct MessageInputContext {
    HWND edit;
    HWND sendButton;
    bool hasChannel;
    std::string channel;
    unsigned long long broadcasterId;
    const std::map<std::wstring, SeenUser> *knownUsers;
    void (*notice)(HWND, const std::wstring &);

    MessageInputContext() : edit(NULL), sendButton(NULL), hasChannel(false),
        broadcasterId(0), knownUsers(NULL), notice(NULL) {}
};

bool copyText(HWND hwnd, const std::wstring &selected);
bool handleMessageInput(HWND hwnd, const MessageInputContext &context);

#endif
