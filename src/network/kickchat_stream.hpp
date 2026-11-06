#ifndef KICKCHAT_STREAM_HPP
#define KICKCHAT_STREAM_HPP
#include "kickchat_platform.hpp"

struct ChannelSession {
    std::string slug;
    volatile LONG active;
    unsigned chatroomId;
    HANDLE thread;
    ChannelSession(const std::string &channel) : slug(channel), active(1), chatroomId(0), thread(NULL) {}
};

void loadPusherConfig();
bool startChannelSession(ChannelSession *session);
void stopChannelSession(ChannelSession *session);
DWORD WINAPI networkThread(LPVOID parameter);
#endif
