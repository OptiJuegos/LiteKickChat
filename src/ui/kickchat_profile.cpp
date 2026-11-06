#include "kickchat_identity.hpp"
#include "kickchat_profile_strings.hpp"
#include "kickchat_profile.hpp"
#include "kickchat_profile_paint.hpp"
#include "kickchat_profile_lookup.hpp"
#include "kickchat_history.hpp"
#include "kickchat_badges.hpp"
#include "kickchat_oauth.hpp"

static const wchar_t *PROFILE_CLASS = kickchatIdentity::profileWindowClass;
static HWND g_profileWindow = NULL;
static ChatRecord g_person;
static std::vector<ChatRecord> g_messages;
static int g_offset = 0;
static HFONT g_profileFont = NULL;
static HFONT g_profileBold = NULL;
static HWND g_kickButton = NULL, g_timeoutButton = NULL;
static HWND g_banButton = NULL, g_unbanButton = NULL;
static std::wstring g_profileStatus;
static std::wstring g_pendingModerationOriginal;
static const int ID_PROFILE_KICK = 5401;
static const int ID_PROFILE_TIMEOUT = 5402;
static const int ID_PROFILE_BAN = 5403;
static const int ID_PROFILE_UNBAN = 5404;

static void updateProfileButtons() {
    const bool moderate = profileLookup::validUsername(g_person.user) && canManageChannel(g_person.channel);
    if (g_timeoutButton) EnableWindow(g_timeoutButton, moderate);
    if (g_banButton) EnableWindow(g_banButton, moderate);
    if (g_unbanButton) EnableWindow(g_unbanButton, moderate);
}

static void layoutProfileButtons(HWND hwnd) {
    RECT client = {};
    GetClientRect(hwnd, &client);
    const int space = 8, left = 20, right = 20;
    const int available = std::max(4, static_cast<int>(client.right) - left - right - 3 * space);
    const int width = available / 4;
    HWND buttons[] = {g_kickButton, g_timeoutButton, g_banButton, g_unbanButton};
    for (int i = 0; i < 4; ++i) {
        if (buttons[i]) MoveWindow(buttons[i], left + i * (width + space), 211,
                                   i == 3 ? std::max(1, static_cast<int>(client.right) - right -
                                        (left + i * (width + space))) : width, 32, TRUE);
    }
}

static void startProfileLookup(HWND hwnd) {
    if (!g_person.id.empty() || !profileLookup::validUsername(g_person.user)) return;
    profileLookup::Request *job = new profileLookup::Request();
    job->window = hwnd;
    job->username = g_person.user;
    HANDLE thread = CreateThread(NULL, 0, profileLookup::worker, job, 0, NULL);
    if (!thread) { delete job; g_profileStatus = kickchatProfileText::get(kickchatProfileText::RemoteIdUnavailable); return; }
    CloseHandle(thread);
    g_profileStatus = kickchatProfileText::get(kickchatProfileText::PublicIdLookup);
}

static bool copyProfileCommand(HWND hwnd, const std::wstring &command) {
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (command.size() + 1) * sizeof(wchar_t));
    if (!memory) return false;
    void *buffer = GlobalLock(memory);
    if (!buffer) { GlobalFree(memory); return false; }
    memcpy(buffer, command.c_str(), (command.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(memory);
    if (!OpenClipboard(hwnd)) { GlobalFree(memory); return false; }
    const bool copied = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory);
    CloseClipboard();
    if (!copied) GlobalFree(memory);
    return copied;
}

static void moderateInKickBrowser(HWND hwnd, kickcmd::Kind kind) {
    const wchar_t *verb = kind == kickcmd::TIMEOUT ? L"/timeout " :
                          kind == kickcmd::BAN ? L"/ban " : L"/unban ";
    const std::wstring command = std::wstring(verb) + toWide(g_person.user) +
                                 (kind == kickcmd::TIMEOUT ? L" 300" : L"");
    if (!copyProfileCommand(hwnd, command)) {
        MessageBoxW(hwnd, kickchatProfileText::get(kickchatProfileText::CommandCopyFailed), L"KickChat", MB_ICONERROR | MB_OK);
        return;
    }
    const std::wstring channelUrl = L"https://kick.com/" + toWide(g_person.channel);
    ShellExecuteW(hwnd, L"open", channelUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
    g_profileStatus = kickchatProfileText::get(kickchatProfileText::CommandCopied);
    InvalidateRect(hwnd, NULL, FALSE);
    MessageBoxW(hwnd, (std::wstring(kickchatProfileText::get(kickchatProfileText::CommandCopyPrefix)) +
        command + kickchatProfileText::get(kickchatProfileText::CommandCopySuffix)).c_str(),
        kickchatProfileText::get(kickchatProfileText::ActionPending), MB_OK | MB_ICONINFORMATION);
}

static void moderateProfile(HWND hwnd, kickcmd::Kind kind) {
    if (!canManageChannel(g_person.channel) || !profileLookup::validUsername(g_person.user)) {
        MessageBoxW(hwnd, kickchatProfileText::get(kickchatProfileText::ModerationUnavailable),
                    L"KickChat", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!profileLookup::validNumericId(g_person.id) || !authenticatedUserId() ||
        InterlockedCompareExchange(&g_authenticated, 0, 0) == 0) {
        moderateInKickBrowser(hwnd, kind);
        return;
    }
    const unsigned long long target = strtoull(g_person.id.c_str(), NULL, 10);
    if (target == authenticatedUserId()) return;
    const wchar_t *verb = kickchatProfileText::get(kind == kickcmd::TIMEOUT ?
        kickchatProfileText::TimeoutConfirm :
        kind == kickcmd::BAN ? kickchatProfileText::BanConfirm : kickchatProfileText::UnbanConfirm);
    if (MessageBoxW(hwnd, (std::wstring(verb) + toWide(g_person.user) + L"?").c_str(),
                    kickchatProfileText::get(kickchatProfileText::ConfirmModeration),
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;
    if (InterlockedCompareExchange(&g_taskBusy, 1, 0) != 0) {
        g_profileStatus = kickchatProfileText::get(kickchatProfileText::AnotherOperation);
        InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    PendingSend *pending = new PendingSend();
    pending->command.kind = kind;
    pending->command.target = toWide(g_person.user);
    pending->command.minutes = kind == kickcmd::TIMEOUT ? 5 : 0;
    pending->command.original = kind == kickcmd::TIMEOUT ? L"/timeout " :
                                kind == kickcmd::BAN ? L"/ban " : L"/unban ";
    pending->command.original += pending->command.target;
    if (kind == kickcmd::TIMEOUT) pending->command.original += L" 5";
    pending->targetId = target;
    pending->broadcasterId = authenticatedUserId();
    pending->channel = g_person.channel;
    g_pendingModerationOriginal = pending->command.original;
    HANDLE thread = CreateThread(NULL, 0, sendWorker, pending, 0, NULL);
    if (!thread) {
        delete pending;
        g_pendingModerationOriginal.clear();
        InterlockedExchange(&g_taskBusy, 0);
        g_profileStatus = kickchatProfileText::get(kickchatProfileText::ModerationStartFailed);
    } else {
        CloseHandle(thread);
        g_profileStatus = kickchatProfileText::get(kickchatProfileText::ModerationProcessing);
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

static LRESULT CALLBACK profileProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
            HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE));
            g_kickButton = CreateWindowExW(0, L"BUTTON", kickchatProfileText::get(kickchatProfileText::ViewOnKick), style,
                0, 0, 1, 1, hwnd, reinterpret_cast<HMENU>(ID_PROFILE_KICK), instance, NULL);
            g_timeoutButton = CreateWindowExW(0, L"BUTTON", kickchatProfileText::get(kickchatProfileText::TimeoutButton), style,
                0, 0, 1, 1, hwnd, reinterpret_cast<HMENU>(ID_PROFILE_TIMEOUT), instance, NULL);
            g_banButton = CreateWindowExW(0, L"BUTTON", kickchatProfileText::get(kickchatProfileText::BanButton), style,
                0, 0, 1, 1, hwnd, reinterpret_cast<HMENU>(ID_PROFILE_BAN), instance, NULL);
            g_unbanButton = CreateWindowExW(0, L"BUTTON", kickchatProfileText::get(kickchatProfileText::UnbanButton), style,
                0, 0, 1, 1, hwnd, reinterpret_cast<HMENU>(ID_PROFILE_UNBAN), instance, NULL);
            HWND buttons[] = {g_kickButton, g_timeoutButton, g_banButton, g_unbanButton};
            for (size_t i = 0; i < 4; ++i)
                if (buttons[i]) SendMessageW(buttons[i], WM_SETFONT, reinterpret_cast<WPARAM>(g_profileFont), TRUE);
            layoutProfileButtons(hwnd);
            updateProfileButtons();
            return 0;
        }
        case profileLookup::Message: {
            profileLookup::Request *result = reinterpret_cast<profileLookup::Request *>(lp);
            if (result && result->username == g_person.user && g_person.id.empty()) {
                if (profileLookup::validNumericId(result->id)) {
                    g_person.id = result->id;
                    g_profileStatus = kickchatProfileText::get(kickchatProfileText::PublicIdFound);
                } else g_profileStatus = kickchatProfileText::get(kickchatProfileText::PublicIdMissing);
                updateProfileButtons();
                InvalidateRect(hwnd, NULL, FALSE);
            }
            delete result;
            return 0;
        }
        case WM_COMMAND: {
            if (HIWORD(wp) != BN_CLICKED) return DefWindowProcW(hwnd, msg, wp, lp);
            const int action = LOWORD(wp);
            if (action == ID_PROFILE_KICK && profileLookup::validUsername(g_person.user)) {
                const std::wstring url = L"https://kick.com/" + toWide(g_person.user);
                ShellExecuteW(hwnd, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                return 0;
            }
            if (action == ID_PROFILE_TIMEOUT || action == ID_PROFILE_BAN || action == ID_PROFILE_UNBAN) {
                moderateProfile(hwnd, action == ID_PROFILE_TIMEOUT ? kickcmd::TIMEOUT :
                                      action == ID_PROFILE_BAN ? kickcmd::BAN : kickcmd::UNBAN);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_PAINT:
            paintUserProfile(hwnd, g_person, g_messages, g_profileStatus,
                             g_profileFont, g_profileBold, g_offset);
            return 0;
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEWHEEL: {
            int delta = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            g_offset = std::max(0, g_offset - delta * 56);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_SIZE: layoutProfileButtons(hwnd); InvalidateRect(hwnd, NULL, FALSE); return 0;
        case WM_GETMINMAXINFO: {
            MINMAXINFO *size = reinterpret_cast<MINMAXINFO *>(lp);
            size->ptMinTrackSize.x = 400;
            size->ptMinTrackSize.y = 450;
            return 0;
        }
        case WM_DESTROY:
            g_profileWindow = NULL;
            g_pendingModerationOriginal.clear();
            g_kickButton = g_timeoutButton = g_banButton = g_unbanButton = NULL;
            DeleteObject(g_profileFont); g_profileFont = NULL;
            DeleteObject(g_profileBold); g_profileBold = NULL;
            return 0;
        default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void showUserProfile(HWND owner, const ChatRecord &record) {
    g_pendingModerationOriginal.clear();
    g_person = record;
    g_messages = recentChatMessages(record.id, 50, record.channel);
    g_offset = 0;
    g_profileStatus = kickchatProfileText::get(g_person.id.empty() ?
        kickchatProfileText::LookingUpUser : kickchatProfileText::ProfileAvailable);
    if (g_profileWindow && IsWindow(g_profileWindow)) {
        SetWindowTextW(g_profileWindow, toWide(record.user).c_str());
        ShowWindow(g_profileWindow, SW_SHOW);
        SetForegroundWindow(g_profileWindow);
        updateProfileButtons();
        startProfileLookup(g_profileWindow);
        InvalidateRect(g_profileWindow, NULL, FALSE);
        return;
    }
    WNDCLASSEXW cls = {0};
    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpfnWndProc = profileProc;
    cls.lpszClassName = PROFILE_CLASS;
    cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&cls);
    g_profileFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_profileBold = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    RECT ownerRect; GetWindowRect(owner, &ownerRect);
    int x = ownerRect.left + (ownerRect.right - ownerRect.left - 440) / 2;
    int y = ownerRect.top + 30;
    g_profileWindow = CreateWindowExW(WS_EX_TOOLWINDOW, PROFILE_CLASS, toWide(record.user).c_str(),
        WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN | WS_VISIBLE,
        x, y, 520, 620, owner, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_profileWindow) {
        DeleteObject(g_profileFont); g_profileFont = NULL;
        DeleteObject(g_profileBold); g_profileBold = NULL;
    } else startProfileLookup(g_profileWindow);
}

void refreshUserProfile(const ChatRecord &record) {
    if (!g_profileWindow || g_person.id != record.id ||
        g_person.channel != record.channel) return;
    g_person = record;
    g_messages = recentChatMessages(record.id, 50, record.channel);
    InvalidateRect(g_profileWindow, NULL, FALSE);
}

void completeUserProfileAction(const SendResult &result) {
    if (!g_profileWindow || g_pendingModerationOriginal.empty() ||
        g_pendingModerationOriginal != result.original) return;
    g_pendingModerationOriginal.clear();
    g_profileStatus = result.message;
    InvalidateRect(g_profileWindow, NULL, FALSE);
}
