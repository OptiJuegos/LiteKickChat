#ifndef KICKCHAT_OAUTH_HPP
#define KICKCHAT_OAUTH_HPP

#include "kickchat_platform.hpp"
#include "kickchat_commands.hpp"

struct HttpResult;

struct PendingSend {
    kickcmd::Command command;
    unsigned long long targetId;
    unsigned long long broadcasterId;
    std::string channel;
    PendingSend() : targetId(0), broadcasterId(0) {}
};

extern volatile LONG g_taskBusy;
extern volatile LONG g_authenticated;
extern volatile LONG g_oauthPending;
extern volatile LONG g_oauthCancelRequested;

void loadTokens();
void resetAuthSession();
bool hasCachedToken();
unsigned long long authenticatedUserId();
std::string authenticatedChannel();
inline bool canManageChannel(const std::string &channel, unsigned long long broadcasterId = 0) {
    const unsigned long long userId = authenticatedUserId();
    return userId != 0 && InterlockedCompareExchange(&g_authenticated, 0, 0) != 0 &&
           !channel.empty() && channel == authenticatedChannel() &&
           (!broadcasterId || broadcasterId == userId);
}
std::string decimalId(unsigned long long value);
bool authorizeCommand(std::wstring &error);
HttpResult kickAuthorized(const wchar_t *path, const wchar_t *method,
                          const std::string &body, std::wstring &error);
void writeApiLog(const std::wstring &entry);
DWORD WINAPI oauthWorker(LPVOID parameter);
DWORD WINAPI sendWorker(LPVOID parameter);

#endif
