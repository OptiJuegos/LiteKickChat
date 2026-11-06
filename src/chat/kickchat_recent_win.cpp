#include "kickchat_recent.hpp"
#include "kickchat_platform.hpp"
#include "kickchat_http.hpp"

static std::string utcTimeOffset(int seconds) {
    FILETIME now = {}, shifted = {};
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER value = {};
    value.LowPart = now.dwLowDateTime;
    value.HighPart = now.dwHighDateTime;
    value.QuadPart += static_cast<LONGLONG>(seconds) * 10000000LL;
    shifted.dwLowDateTime = value.LowPart;
    shifted.dwHighDateTime = value.HighPart;
    SYSTEMTIME utc = {};
    if (!FileTimeToSystemTime(&shifted, &utc)) return "";
    char text[24];
    sprintf(text, "%04u-%02u-%02uT%02u:%02u:%02u", utc.wYear,
            utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond);
    return text;
}

static bool localTimeFromUtc(const std::string &utc, std::string &localTime) {
    unsigned year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (sscanf(utc.c_str(), "%4u-%2u-%2uT%2u:%2u:%2u",
               &year, &month, &day, &hour, &minute, &second) != 6) return false;
    SYSTEMTIME source = {};
    source.wYear = static_cast<WORD>(year);
    source.wMonth = static_cast<WORD>(month);
    source.wDay = static_cast<WORD>(day);
    source.wHour = static_cast<WORD>(hour);
    source.wMinute = static_cast<WORD>(minute);
    source.wSecond = static_cast<WORD>(second);
    FILETIME utcFile = {}, localFile = {};
    SYSTEMTIME local = {};
    if (!SystemTimeToFileTime(&source, &utcFile) ||
        !FileTimeToLocalFileTime(&utcFile, &localFile) ||
        !FileTimeToSystemTime(&localFile, &local)) return false;
    char buffer[32];
    sprintf(buffer, "%04u-%02u-%02u %02u:%02u:%02u",
            local.wYear, local.wMonth, local.wDay,
            local.wHour, local.wMinute, local.wSecond);
    localTime = buffer;
    return true;
}

void fetchRecentKickMessages(const std::string &channel, unsigned chatroomId) {
    if (!chatroomId || !running()) return;
    char id[32];
    sprintf(id, "%u", chatroomId);
    const std::wstring url = L"https://web.kick.com/api/v1/chat/" + toWide(id) + L"/history";
    std::string response;
    if (!httpGet(url, response, 1024 * 1024)) return;
    const std::string start = utcTimeOffset(-300);
    const std::string end = utcTimeOffset(5);
    if (start.empty() || end.empty()) return;
    std::vector<ChatRecord> messages;
    if (!parseRecentKickMessages(response, channel, start, end, messages)) return;
    for (size_t i = 0; i < messages.size() && running(); ++i) {
        std::string localTime;
        if (!localTimeFromUtc(messages[i].receivedAt, localTime)) continue;
        messages[i].receivedAt = localTime;
        addChatRecord(messages[i], chatColor(messages[i].color), channel, true);
    }
}
