#include "kickchat_identity.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_history.hpp"
#include "kickchat_platform.hpp"
#include "kickchat_commands.hpp"
#include <shlobj.h>

static const size_t HISTORY_LIMIT = 3500;
static const size_t HISTORY_KEEP = 3000;
static const DWORD HISTORY_FILE_LIMIT = 16 * 1024 * 1024;
static std::deque<ChatRecord> g_history;
static std::wstring g_historyPath;

static bool getHistoryPath(bool create) {
    wchar_t folder[MAX_PATH] = {0};
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, folder) != S_OK)
        return false;
    std::wstring directory = std::wstring(folder) + L"\\KickChat";
    if (create && !CreateDirectoryW(directory.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    g_historyPath = directory + L"\\history.jsonl";
    return true;
}

static bool writeRecord(HANDLE file, const ChatRecord &record) {
    std::string line = encodeChatRecord(record) + "\n";
    DWORD written = 0;
    return WriteFile(file, line.data(), static_cast<DWORD>(line.size()), &written, NULL) &&
           written == line.size();
}

static void compactHistory() {
    if (g_historyPath.empty()) return;
    while (g_history.size() > HISTORY_KEEP) g_history.pop_front();
    const std::wstring temp = g_historyPath + L".tmp";
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    bool ok = true;
    for (size_t i = 0; i < g_history.size(); ++i)
        if (!writeRecord(file, g_history[i])) { ok = false; break; }
    if (ok) ok = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    if (ok) ok = MoveFileExW(temp.c_str(), g_historyPath.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!ok) {
        DeleteFileW(temp.c_str());
        recordError(kickchatText::get(kickchatText::HistoryStage),
                    kickchatText::get(kickchatText::HistoryCompactFailed));
    }
}

void loadChatHistory() {
    g_history.clear();
    if (!getHistoryPath(true)) return;
    HANDLE file = CreateFileW(g_historyPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD bytes = GetFileSize(file, NULL);
    if (bytes == INVALID_FILE_SIZE || bytes > HISTORY_FILE_LIMIT) {
        CloseHandle(file);
        recordError(L"Historial", L"El archivo de historial supera el limite de lectura de 16 MiB");
        return;
    }
    std::string data(bytes, '\0');
    DWORD read = 0;
    bool ok = bytes == 0 || (ReadFile(file, &data[0], bytes, &read, NULL) != 0 && read == bytes);
    CloseHandle(file);
    if (!ok) return;
    size_t begin = 0;
    while (begin < data.size()) {
        size_t end = data.find('\n', begin);
        if (end == std::string::npos) break;
        ChatRecord record;
        if (decodeChatRecord(data.substr(begin, end - begin), record)) {
            g_history.push_back(record);
            if (g_history.size() > HISTORY_LIMIT) g_history.pop_front();
        }
        begin = end + 1;
    }
    if (g_history.size() >= HISTORY_LIMIT) compactHistory();
}

void appendChatHistory(const ChatRecord &record) {
    if (record.id.empty() || record.content.empty()) return;
    if (!record.messageId.empty()) {
        for (std::deque<ChatRecord>::const_reverse_iterator it = g_history.rbegin();
             it != g_history.rend(); ++it) {
            if (it->channel == record.channel && it->messageId == record.messageId) return;
        }
    }
    ChatRecord saved = record;
    if (saved.content.size() > 1200) {
        saved.content.resize(1200);
        while (!saved.content.empty() &&
               (static_cast<unsigned char>(saved.content[saved.content.size() - 1]) & 0xC0) == 0x80)
            saved.content.resize(saved.content.size() - 1);
        if (!saved.content.empty() &&
            (static_cast<unsigned char>(saved.content[saved.content.size() - 1]) & 0xC0) == 0xC0)
            saved.content.resize(saved.content.size() - 1);
        saved.content += "...";
    }
    if (g_historyPath.empty() && !getHistoryPath(true)) return;
    HANDLE file = CreateFileW(g_historyPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, NULL,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        recordError(kickchatText::get(kickchatText::HistoryStage),
                    kickchatText::get(kickchatText::HistoryOpenFailed));
        return;
    }
    bool ok = writeRecord(file, saved);
    CloseHandle(file);
    if (!ok) {
        recordError(kickchatText::get(kickchatText::HistoryStage),
                    kickchatText::get(kickchatText::HistorySaveFailed));
        return;
    }
    g_history.push_back(saved);
    if (g_history.size() >= HISTORY_LIMIT) compactHistory();
}

bool findChatUser(const std::wstring &name, ChatRecord &latest, const std::string &channel) {
    const std::wstring wanted = kickcmd::lower(name);
    for (std::deque<ChatRecord>::const_reverse_iterator it = g_history.rbegin(); it != g_history.rend(); ++it) {
        if ((channel.empty() || it->channel == channel) && kickcmd::lower(toWide(it->user)) == wanted) {
            latest = *it;
            return true;
        }
    }
    return false;
}

std::vector<ChatRecord> recentChatMessages(const std::string &id, size_t limit,
                                            const std::string &channel) {
    std::vector<ChatRecord> found;
    if (id.empty() || !limit) return found;
    for (std::deque<ChatRecord>::const_reverse_iterator it = g_history.rbegin(); it != g_history.rend(); ++it) {
        if (it->id == id && (channel.empty() || it->channel == channel)) found.push_back(*it);
        if (found.size() >= limit) break;
    }
    return found;
}

std::vector<ChatRecord> recentChannelHistory(const std::string &channel,
                                             const std::string &cutoff, size_t limit) {
    std::vector<ChatRecord> records;
    if (!limit) return records;
    for (std::deque<ChatRecord>::const_iterator it = g_history.begin();
         it != g_history.end(); ++it) {
        if (it->channel == channel && it->receivedAt >= cutoff)
            records.push_back(*it);
    }
    std::stable_sort(records.begin(), records.end(),
                     [](const ChatRecord &a, const ChatRecord &b) {
                         return a.receivedAt < b.receivedAt;
                     });
    if (records.size() > limit)
        records.erase(records.begin(), records.end() - limit);
    return records;
}
