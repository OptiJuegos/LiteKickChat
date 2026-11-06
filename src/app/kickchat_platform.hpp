#ifndef KICKCHAT_PLATFORM_HPP
#define KICKCHAT_PLATFORM_HPP

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define SECURITY_WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <security.h>
#include <schannel.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <cstring>
#include "kickchat_chat_data.hpp"

const UINT WM_CHAT = WM_APP + 10;
const UINT WM_STATUS = WM_APP + 11;
const UINT WM_AUTH_CHANGED = WM_APP + 12;
const UINT WM_SENT = WM_APP + 13;
const UINT WM_EMOTE_CATALOG = WM_APP + 14;
const UINT WM_EMOTE_IMAGE = WM_APP + 15;
const UINT WM_CHANNEL_ID = WM_APP + 16;

struct ChatLine {
    std::wstring user, content;
    std::string channel;
    COLORREF color;
    unsigned long long userId;
    ChatRecord record;
    bool backfill;
    ChatLine() : color(RGB(75, 220, 125)), userId(0), backfill(false) {}
};
struct UiPacket { ChatLine chat; std::wstring status; std::string channel; unsigned long long broadcasterId; UiPacket() : broadcasterId(0) {} };
struct SendResult { std::wstring original, message; std::string channel; bool success; };

extern volatile LONG g_running;
extern HWND g_window;
extern std::wstring g_lastError;

bool running();
void uiPost(UINT msg, UiPacket *packet);
void setStatus(const std::wstring &status);
void setChannelStatus(const std::string &channel, const std::wstring &status);
void setChannelBroadcaster(const std::string &channel, unsigned long long userId);
void addChat(const std::string &user, const std::string &content, COLORREF color,
             unsigned long long userId = 0, const std::string &channel = "");
void addChatRecord(const ChatRecord &record, COLORREF color, const std::string &channel = "",
                   bool backfill = false);
COLORREF chatColor(const std::string &color);
std::wstring toWide(const std::string &text);
std::string utf8FromWide(const std::wstring &text);
void recordError(const wchar_t *stage, const std::wstring &cause);
void recordCode(const wchar_t *stage, DWORD code);
bool randomBytes(BYTE *destination, DWORD count);
std::string base64(const BYTE *data, size_t count);
std::wstring exeFile(const wchar_t *name);

#endif
