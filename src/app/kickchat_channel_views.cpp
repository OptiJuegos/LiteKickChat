#include "kickchat_identity.hpp"
#include "kickchat_channel_views.hpp"
#include "kickchat_history.hpp"
#include "kickchat_channels.hpp"
#include "kickchat_oauth.hpp"
#include "kickchat_emotes.hpp"

static const size_t MAX_MESSAGES = 250;

static std::string fiveMinuteCutoff() {
    FILETIME now = {}, cutoff = {}, localFile = {};
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER value = {};
    value.LowPart = now.dwLowDateTime;
    value.HighPart = now.dwHighDateTime;
    value.QuadPart -= 300ULL * 10000000ULL;
    cutoff.dwLowDateTime = value.LowPart;
    cutoff.dwHighDateTime = value.HighPart;
    SYSTEMTIME local = {};
    if (!FileTimeToLocalFileTime(&cutoff, &localFile) ||
        !FileTimeToSystemTime(&localFile, &local)) return "";
    char buffer[24];
    sprintf(buffer, "%04u-%02u-%02u %02u:%02u:%02u", local.wYear,
            local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
    return buffer;
}

static void preloadLocalHistory(ChannelView &view) {
    const std::string cutoff = fiveMinuteCutoff();
    if (cutoff.empty()) return;
    const std::vector<ChatRecord> records = recentChannelHistory(view.slug, cutoff, MAX_MESSAGES);
    for (size_t i = 0; i < records.size(); ++i) {
        ChatLine line;
        line.user = toWide(records[i].user);
        line.content = toWide(records[i].content);
        line.channel = view.slug;
        line.color = chatColor(records[i].color);
        line.userId = strtoull(records[i].id.c_str(), NULL, 10);
        line.record = records[i];
        view.lines.push_back(line);
    }
}

void ChannelViews::saveChannelList() const {
    std::vector<std::string> names;
    for (size_t i = 0; i < channels.size(); ++i) names.push_back(channels[i].slug);
    if (!saveChannels(names)) recordError(L"Canales", kickchatText::get(kickchatText::ChannelListSaveError));
}

int ChannelViews::index(const std::string &slug) const {
    for (size_t i = 0; i < channels.size(); ++i)
        if (channels[i].slug == slug) return static_cast<int>(i);
    return -1;
}

void ChannelViews::select(HWND window, int index, ChannelUiState &ui) {
    if (index < 0 || index >= static_cast<int>(channels.size()) || index == active) return;
    ui.hover.clear(window);
    if (active >= 0) {
        channels[active].lines.swap(ui.lines);
        channels[active].scroll = ui.scroll;
        int length = GetWindowTextLengthW(ui.messageEdit);
        std::wstring current(length + 1, L'\0');
        GetWindowTextW(ui.messageEdit, &current[0], length + 1);
        current.resize(length);
        channels[active].draft = current;
    }
    if (active < 0) ui.lines.clear();
    active = index;
    if (canManageChannel(channels[active].slug))
        channels[active].broadcasterId = authenticatedUserId();
    ui.lines.swap(channels[active].lines);
    ui.selection = ChatTextSelection();
    ui.scroll = channels[active].scroll;
    ui.status = channels[active].status;
    SetWindowTextW(ui.messageEdit, channels[active].draft.c_str());
    activateEmoteChannel(channels[active].slug);
    EnableWindow(ui.sendButton, InterlockedCompareExchange(&g_authenticated, 0, 0) != 0 &&
                 InterlockedCompareExchange(&g_taskBusy, 0, 0) == 0 && channels[active].broadcasterId != 0);
    InvalidateRect(window, NULL, FALSE);
}

bool ChannelViews::add(HWND window, const std::string &slug, bool persist, ChannelUiState &ui) {
    if (index(slug) >= 0) {
        select(window, index(slug), ui);
        return false;
    }
    if (channels.size() >= 8) return false;
    ChannelSession *session = new ChannelSession(slug);
    if (channels.empty()) session->chatroomId = manualChatId;
    if (!startChannelSession(session)) {
        delete session;
        return false;
    }
    sessions.push_back(session);
    ChannelView view(slug);
    preloadLocalHistory(view);
    view.session = session;
    channels.push_back(view);
    select(window, static_cast<int>(channels.size()) - 1, ui);
    if (persist) saveChannelList();
    return true;
}

void ChannelViews::remove(HWND window, int index, ChannelUiState &ui) {
    if (index < 0 || index >= static_cast<int>(channels.size())) return;
    ui.hover.clear(window);
    const std::string slug = channels[index].slug;
    std::wstring question = std::wstring(kickchatText::get(kickchatText::ChannelRemovePrompt)) +
                             toWide(slug) + kickchatText::get(kickchatText::ChannelRemoveSuffix);
    if (MessageBoxW(window, question.c_str(), kickchatText::get(kickchatText::ChannelRemoveTitle),
                    MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    if (active >= 0) {
        channels[active].lines.swap(ui.lines);
        channels[active].scroll = ui.scroll;
        active = -1;
    }
    SetWindowTextW(ui.messageEdit, L"");
    stopChannelSession(channels[index].session);
    seenUsers.erase(slug);
    channels.erase(channels.begin() + index);
    ui.lines.clear();
    ui.selection = ChatTextSelection();
    ui.scroll = 0;
    ui.status = kickchatText::get(kickchatText::InitialStatus);
    if (channels.empty()) activateEmoteChannel("");
    if (!channels.empty()) select(window, std::min(index, static_cast<int>(channels.size()) - 1), ui);
    if (channels.empty()) EnableWindow(ui.sendButton, FALSE);
    saveChannelList();
    InvalidateRect(window, NULL, FALSE);
}



void ChannelViews::shutdown() {
    for (size_t i=0; i<sessions.size(); ++i) stopChannelSession(sessions[i]);
    for (size_t i=0; i<sessions.size(); ++i) {
        DWORD result = WaitForSingleObject(sessions[i]->thread, 1500);
        CloseHandle(sessions[i]->thread);
        if (result == WAIT_OBJECT_0) delete sessions[i];
    }
}
