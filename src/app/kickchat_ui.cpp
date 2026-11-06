#include "kickchat_identity.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_platform.hpp"
#include "kickchat_message_input.hpp"
#include "kickchat_main_controls.hpp"
#include "kickchat_chat_selection.hpp"
#include "kickchat_channel_views.hpp"
#include "kickchat_chat_events.hpp"
#include "kickchat_main_window_events.hpp"
#include "kickchat_stream.hpp"
#include "kickchat_oauth.hpp"
#include "kickchat_credentials.hpp"
#include "kickchat_settings.hpp"
#include "kickchat_buttons.hpp"
#include "kickchat_badges.hpp"
#include "kickchat_history.hpp"
#include "kickchat_profile.hpp"
#include "kickchat_emotes.hpp"
#include "kickchat_chat_paint.hpp"
#include "kickchat_link_hover.hpp"
#include "kickchat_emote_tokens.hpp"
#include "kickchat_channels.hpp"
#include "kickchat_preferences.hpp"
#include "kickchat_channel_dialog.hpp"
#include "kickchat_edit_keys.hpp"
#include <propidl.h>
#include <gdiplus.h>
#include <windowsx.h>

static const wchar_t *WINDOW_CLASS = kickchatIdentity::mainWindowClass;
static HFONT g_font = NULL, g_bold = NULL;
static HBRUSH g_messageBrush = NULL;
static MainControls g_controls;
static int g_scrollMax = 0;
static int g_chatPage = 1;
static std::wstring g_status = kickchatText::get(kickchatText::Initializing);
static std::wstring g_authStatus = kickchatText::get(kickchatText::NotAuthorized);
static int g_scroll = 0;
static VisualPreferences g_visualPreferences;
static std::deque<ChatLine> g_lines;
static ChatSelectionController g_chatSelection;
static LinkHoverController g_hover;
static const int SIDEBAR_WIDTH = mainWindowEvents::SidebarWidth;
static ChannelViews g_channelViews;

static ChannelUiState channelUiState() {
    return ChannelUiState(g_lines, g_scroll, g_chatSelection.selection, g_status,
                          g_controls.messageEdit, g_controls.sendButton, g_hover);
}

static int channelIndex(const std::string &slug) {
    return g_channelViews.index(slug);
}

static void selectChannel(HWND window, int index) {
    ChannelUiState ui=channelUiState();
    g_channelViews.select(window, index, ui);
}

static bool addChannel(HWND window, const std::string &slug, bool persist) {
    ChannelUiState ui=channelUiState();
    return g_channelViews.add(window, slug, persist, ui);
}

static void removeChannel(HWND window, int index) {
    ChannelUiState ui=channelUiState();
    g_channelViews.remove(window, index, ui);
}

static void uiNotice(HWND hwnd,const std::wstring &note) {
    g_authStatus=note;InvalidateRect(hwnd,NULL,FALSE);
    addChat(utf8FromWide(kickchatText::get(kickchatText::SystemLabel)),
            utf8FromWide(note),RGB(128,201,255));
}
static void sendTypedMessage(HWND hwnd) {
    MessageInputContext context;
    context.edit = g_controls.messageEdit;
    context.sendButton = g_controls.sendButton;
    context.hasChannel = g_channelViews.active >= 0;
    context.channel = context.hasChannel ? g_channelViews.channels[g_channelViews.active].slug : authenticatedChannel();
    context.broadcasterId = context.hasChannel ? g_channelViews.channels[g_channelViews.active].broadcasterId : 0;
    context.knownUsers = &g_channelViews.seenUsers[context.channel];
    context.notice = uiNotice;
    if (handleMessageInput(hwnd, context)) {
        g_authStatus=kickchatText::get(kickchatText::Running);
        InvalidateRect(hwnd,NULL,FALSE);
    }
}
static LRESULT CALLBACK editProc(HWND edit,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_KEYDOWN&&wp==VK_RETURN){sendTypedMessage(GetParent(edit));return 0;}
    if (msg == WM_KEYDOWN &&
        handleEditControlShortcut(edit, wp, (GetKeyState(VK_CONTROL) & 0x8000) != 0))
        return 0;
    if (msg == WM_CHAR && (GetKeyState(VK_CONTROL) & 0x8000) &&
        (wp == 1 || wp == 127)) return 0;
    return CallWindowProcW(g_controls.originalEditProc,edit,msg,wp,lp);
}
static void startAuthorization(HWND hwnd) {
    if (InterlockedCompareExchange(&g_taskBusy, 0, 0) != 0) return;
    std::string id, secret;
    const bool configured = loadCredentials(id, secret);
    if (!secret.empty()) SecureZeroMemory(&secret[0], secret.size());
    if (!configured) {
        uiNotice(hwnd, kickchatText::get(kickchatText::ConfigureCredentials));
        return;
    }
    if (InterlockedCompareExchange(&g_taskBusy, 1, 0) != 0) return;
    InterlockedExchange(&g_oauthCancelRequested, 0);
    InterlockedExchange(&g_oauthPending, 1);
    HANDLE thread = CreateThread(NULL, 0, oauthWorker, NULL, 0, NULL);
    if (thread) {
        CloseHandle(thread);
        EnableWindow(g_controls.sendButton, FALSE);
        g_authStatus = kickchatText::get(kickchatText::WaitingAuthorization);
        InvalidateRect(hwnd, NULL, FALSE);
    } else {
        InterlockedExchange(&g_oauthPending, 0);
        InterlockedExchange(&g_taskBusy, 0);
        uiNotice(hwnd, kickchatText::get(kickchatText::AuthorizeFailed));
    }
}

static void configureCredentials(HWND hwnd) {
    bool changed = false;
    bool authorize = false;
    g_hover.clear(hwnd);
    if (!showCredentialSettings(hwnd, changed, authorize, g_visualPreferences)) return;
    kickchatText::setLanguage(g_visualPreferences.language);
    SetWindowTextW(hwnd, kickchatText::get(kickchatText::MainTitle));
    SetWindowTextW(g_controls.addButton, kickchatText::get(kickchatText::AddChannel));
    SetWindowTextW(g_controls.settingsButton, kickchatText::get(kickchatText::SettingsButton));
    SetWindowTextW(g_controls.sendButton, kickchatText::get(kickchatText::SendButton));
    setAnimatedEmotesEnabled(!g_visualPreferences.disableGifs);
    InvalidateRect(hwnd, NULL, FALSE);
    SetWindowPos(hwnd, g_visualPreferences.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (changed) resetAuthSession();
    const bool authorized = InterlockedCompareExchange(&g_authenticated, 0, 0) != 0;
    g_authStatus = kickchatText::get(changed ? kickchatText::CredentialsSavedConnect :
                                               kickchatText::CredentialsSaved);
    EnableWindow(g_controls.sendButton, authorized && g_channelViews.active >= 0 &&
                 g_channelViews.channels[g_channelViews.active].broadcasterId != 0);
    InvalidateRect(hwnd, NULL, FALSE);
    if (authorize) startAuthorization(hwnd);
}

static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    mainWindowEvents::ChatView view = {
        g_controls, g_channelViews, g_chatSelection, g_hover, g_visualPreferences,
        g_lines, g_status, g_authStatus, g_font, g_bold,
        g_scroll, g_scrollMax, g_chatPage
    };
    LRESULT eventResult = 0;
    if (mainWindowEvents::handleChatEvent(hwnd, msg, wp, lp, view, eventResult))
        return eventResult;
    switch(msg) {
        case LinkHoverController::Message: {
            g_hover.complete(hwnd, lp, g_visualPreferences.linkPreviews);
            return 0;
        }
        case WM_CHAT: {
            UiPacket *packet=(UiPacket*)lp;
            if (packet) {
                receiveChatPacket(*packet, g_channelViews, g_lines, g_chatSelection.selection, g_scroll);
                delete packet;
                InvalidateRect(hwnd,NULL,FALSE);
            }
            return 0;
        }
        case WM_EMOTE_CATALOG: {
            applyEmoteCatalog(reinterpret_cast<EmoteCatalogDownload *>(lp));
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_EMOTE_IMAGE: {
            applyEmoteDownload(reinterpret_cast<EmoteDownload *>(lp));
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        case WM_CHANNEL_ID: {
            UiPacket *packet=reinterpret_cast<UiPacket *>(lp);
            if (packet) {
                receiveChannelIdPacket(*packet, g_channelViews, g_controls.sendButton);
                delete packet;
            }
            return 0;
        }
        case WM_STATUS: {
            UiPacket *packet=(UiPacket*)lp;
            if (packet) {
                receiveStatusPacket(*packet, g_channelViews, g_status);
                delete packet;
                InvalidateRect(hwnd,NULL,FALSE);
            }
            return 0;
        }
        case WM_AUTH_CHANGED: {
            UiPacket *p=(UiPacket*)lp;
            if(p){g_authStatus=p->status;delete p;}
            bool ready=InterlockedCompareExchange(&g_authenticated,0,0)!=0;
            if (ready && g_channelViews.active >= 0 &&
                canManageChannel(g_channelViews.channels[g_channelViews.active].slug))
                g_channelViews.channels[g_channelViews.active].broadcasterId = authenticatedUserId();
            if (ready && g_channelViews.active >= 0 && g_channelViews.channels[g_channelViews.active].broadcasterId)
                requestSevenTvChannel(g_channelViews.channels[g_channelViews.active].broadcasterId, g_channelViews.channels[g_channelViews.active].slug);
            bool idle=InterlockedCompareExchange(&g_taskBusy,0,0)==0;
            LONG oauthStage=InterlockedCompareExchange(&g_oauthPending,0,0);
            if(g_controls.sendButton)EnableWindow(g_controls.sendButton,ready&&idle&&g_channelViews.active>=0 &&
                g_channelViews.channels[g_channelViews.active].broadcasterId!=0);
            if(g_controls.settingsButton) EnableWindow(g_controls.settingsButton, idle || oauthStage != 0);
            InvalidateRect(hwnd,NULL,FALSE);return 0;
        }
        case WM_SENT: {
            SendResult *packet=(SendResult*)lp;
            if(packet){
                receiveSendResult(*packet, g_channelViews, g_authStatus, g_controls.messageEdit);
                delete packet;
            }
            EnableWindow(g_controls.sendButton,InterlockedCompareExchange(&g_authenticated,1,1)!=0 &&
                g_channelViews.active>=0 && g_channelViews.channels[g_channelViews.active].broadcasterId!=0);
            EnableWindow(g_controls.settingsButton,TRUE);
            InvalidateRect(hwnd,NULL,FALSE);return 0;
        }
        case WM_COMMAND: {
            if (LOWORD(wp) == ID_ADD_CHANNEL && HIWORD(wp) == BN_CLICKED) {
                std::string slug;
                if (promptChannel(hwnd, slug)) {
                    if (!addChannel(hwnd, slug, true) && channelIndex(slug) < 0)
                        uiNotice(hwnd, kickchatText::get(kickchatText::AddChannelFailed));
                }
                return 0;
            }
            if (LOWORD(wp) == ID_SETTINGS && HIWORD(wp) == BN_CLICKED) {
                const LONG pending = InterlockedCompareExchange(&g_oauthPending, 0, 0);
                if (InterlockedCompareExchange(&g_taskBusy, 0, 0) == 0 || pending != 0)
                    configureCredentials(hwnd);
                return 0;
            }
            if(LOWORD(wp)==ID_SEND&&HIWORD(wp)==BN_CLICKED){sendTypedMessage(hwnd);return 0;}
            return DefWindowProcW(hwnd,msg,wp,lp);
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            g_hover.clear(hwnd);
            ChatRecord user;
            if (chatUserAt(x, y, user)) {
                showUserProfile(hwnd, user);
                return 0;
            }
            if (x < SIDEBAR_WIDTH && y >= 10) {
                int index = (y - 10) / 42;
                if (index >= 0 && index < static_cast<int>(g_channelViews.channels.size()) && y < 10 + (index+1)*42) {
                    if (x >= SIDEBAR_WIDTH - 30) removeChannel(hwnd, index);
                    else selectChannel(hwnd, index);
                    return 0;
                }
            }
            g_chatSelection.mouseDown(hwnd, x, y);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO *info = (MINMAXINFO *)lp;
            info->ptMinTrackSize.x = 540;
            info->ptMinTrackSize.y = 400;
            return 0;
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            SetTextColor(dc, RGB(239, 243, 245));
            SetBkColor(dc, RGB(37, 42, 48));
            return (LRESULT)g_messageBrush;
        }
        case WM_DRAWITEM:
            if(drawActionButton((const DRAWITEMSTRUCT *)lp,g_font))return TRUE;
            return DefWindowProcW(hwnd,msg,wp,lp);
        case WM_ERASEBKGND:return 1;
        case WM_CLOSE: g_hover.clear(hwnd);DestroyWindow(hwnd);return 0;
        case WM_DESTROY:
            g_hover.destroy(hwnd);
            InterlockedExchange(&g_running,0);g_window=NULL;PostQuitMessage(0);return 0;
        default:return DefWindowProcW(hwnd,msg,wp,lp);
    }
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR cmd,int) {
    // A numeric command-line argument overrides the saved chatroom ID.
    unsigned manual=0;
    if (cmd && *cmd) {
        while (*cmd==L' '||*cmd==L'\t') ++cmd;
        wchar_t *end=NULL;unsigned long n=wcstoul(cmd,&end,10);
        if (end!=cmd && n>0&&n<=0x7FFFFFFFul) manual=(unsigned)n;
    }
    WNDCLASSEXW cls={0};cls.cbSize=sizeof(cls);cls.hInstance=instance;cls.lpfnWndProc=proc;
    cls.lpszClassName=WINDOW_CLASS;cls.hCursor=LoadCursor(NULL,IDC_ARROW);
    cls.hIcon=LoadIcon(NULL,IDI_APPLICATION);cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    if (!RegisterClassExW(&cls)) return 1;
    g_messageBrush = CreateSolidBrush(RGB(37, 42, 48));
    g_font=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    g_bold=CreateFontW(-17,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    g_window=CreateWindowExW(0,WINDOW_CLASS,kickchatText::get(kickchatText::MainTitle),WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_VISIBLE,
        CW_USEDEFAULT,CW_USEDEFAULT,540,650,NULL,NULL,instance,NULL);
    if (!g_window) {DeleteObject(g_font);DeleteObject(g_bold);DeleteObject(g_messageBrush);return 1;}
    Gdiplus::GdiplusStartupInput graphicsInput;
    ULONG_PTR graphicsToken = 0;
    const bool graphicsReady = Gdiplus::GdiplusStartup(&graphicsToken, &graphicsInput, NULL) == Gdiplus::Ok;
    loadVisualPreferences(g_visualPreferences);
    kickchatText::setLanguage(g_visualPreferences.language);
    SetWindowTextW(g_window, kickchatText::get(kickchatText::MainTitle));
    setAnimatedEmotesEnabled(!g_visualPreferences.disableGifs);
    SetWindowPos(g_window, g_visualPreferences.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    createMainControls(g_window,instance,g_font,editProc,g_controls);
    if (graphicsReady) startEmoteService(g_window);
    loadChatHistory();
    loadTokens();
    std::string configuredId, configuredSecret;
    bool hasConfiguration = loadCredentials(configuredId, configuredSecret);
    if (!configuredSecret.empty()) SecureZeroMemory(&configuredSecret[0], configuredSecret.size());
    if (!hasConfiguration) g_authStatus = kickchatText::get(kickchatText::ConfigureCredentials);
    if(hasConfiguration && hasCachedToken()) {
        g_authStatus=kickchatText::get(kickchatText::SavedSession);
        // Verify account asynchronously without needing a new OAuth grant.
        // Reauthorization remains available in Settings if token validation fails.
        InterlockedExchange(&g_authenticated,1);
        EnableWindow(g_controls.sendButton,TRUE);
    }
    g_channelViews.manualChatId = manual;
    loadPusherConfig();
    g_status = kickchatText::get(kickchatText::InitialStatus);
    std::vector<std::string> savedChannels;
    if (loadChannels(savedChannels)) {
        for (size_t i=0; i<savedChannels.size(); ++i) addChannel(g_window, savedChannels[i], false);
    }
    SetTimer(g_window, 1, 70, NULL);
    MSG msg;
    while (GetMessageW(&msg,NULL,0,0)>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    InterlockedExchange(&g_running,0);
    KillTimer(g_window, 1);
    g_channelViews.shutdown();
    stopEmoteService();
    if (graphicsToken) Gdiplus::GdiplusShutdown(graphicsToken);
    DeleteObject(g_font);DeleteObject(g_bold);DeleteObject(g_messageBrush);
    return 0;
}
