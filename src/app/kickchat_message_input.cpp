#include "kickchat_identity.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_message_input.hpp"
#include "kickchat_oauth.hpp"
#include "kickchat_profile.hpp"
#include "kickchat_history.hpp"

static void showHelp() {
    const std::string label = utf8FromWide(kickchatText::get(kickchatText::HelpLabel));
    const kickchatText::Key help[] = {
        kickchatText::HelpTitle, kickchatText::HelpGame, kickchatText::HelpBan,
        kickchatText::HelpTimeout, kickchatText::HelpUnban, kickchatText::HelpUser,
        kickchatText::HelpRoles
    };
    for (size_t i = 0; i < sizeof(help) / sizeof(help[0]); ++i)
        addChat(label, utf8FromWide(kickchatText::get(help[i])), RGB(128,201,255));
}
bool copyText(HWND hwnd, const std::wstring &selected) {
    HGLOBAL buffer = GlobalAlloc(GMEM_MOVEABLE, (selected.size() + 1) * sizeof(wchar_t));
    if (!buffer) return false;
    wchar_t *bytes = static_cast<wchar_t *>(GlobalLock(buffer));
    if (!bytes) { GlobalFree(buffer); return false; }
    memcpy(bytes, selected.c_str(), (selected.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(buffer);
    if (!OpenClipboard(hwnd)) { GlobalFree(buffer); return false; }
    bool copied = false;
    if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, buffer)) copied = true;
    CloseClipboard();
    if (!copied) GlobalFree(buffer);
    return copied;
}
static bool isRoleCommand(kickcmd::Kind kind) {
    return kind == kickcmd::MOD || kind == kickcmd::UNMOD ||
           kind == kickcmd::VIP || kind == kickcmd::UNVIP;
}
static void prepareRoleCommand(HWND hwnd, const MessageInputContext &context,
                               const kickcmd::Command &command) {
    if (!canManageChannel(context.channel, context.broadcasterId)) {
        context.notice(hwnd, kickchatText::get(kickchatText::OwnRolesOnly));
        return;
    }
    if (!kickcmd::validUsername(command.target)) {
        context.notice(hwnd, kickchatText::get(kickchatText::InvalidRoleName));
        return;
    }
    const wchar_t *verb = command.kind == kickcmd::MOD ? L"/mod" :
                          command.kind == kickcmd::UNMOD ? L"/unmod" :
                          command.kind == kickcmd::VIP ? L"/vip" : L"/unvip";
    const std::wstring prepared = std::wstring(verb) + L" " + command.target;
    if (!copyText(hwnd, prepared)) {
        context.notice(hwnd, kickchatText::get(kickchatText::ClipboardError));
        return;
    }
    const std::wstring channelUrl = L"https://kick.com/" + toWide(context.channel);
    const HINSTANCE browser = ShellExecuteW(hwnd, L"open", channelUrl.c_str(),
                                            NULL, NULL, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(browser) <= 32) {
        context.notice(hwnd, std::wstring(kickchatText::get(kickchatText::CopiedPrefix)) +
                       prepared + kickchatText::get(kickchatText::CopiedOpenBrowser));
    } else {
        context.notice(hwnd, std::wstring(kickchatText::get(kickchatText::CopiedPrefix)) +
                       prepared + kickchatText::get(kickchatText::CopiedPasteWeb));
    }
    SetWindowTextW(context.edit, L"");
}
bool handleMessageInput(HWND hwnd, const MessageInputContext &context) {
    if(!context.edit)return false;
    int len=GetWindowTextLengthW(context.edit);
    if(len<=0||len>1000)return false;
    std::wstring text(len+1,L'\0');GetWindowTextW(context.edit,&text[0],len+1);
    text.resize(wcslen(text.c_str()));
    kickcmd::Command command=kickcmd::parse(text);
    if(command.original.empty())return false;
    if (!context.hasChannel && command.kind != kickcmd::HELP && command.kind != kickcmd::USER) {
        context.notice(hwnd, kickchatText::get(kickchatText::NeedChannel));
        return false;
    }
    if(command.kind==kickcmd::INVALID){context.notice(hwnd,command.error);return false;}
    if(command.kind==kickcmd::HELP){showHelp();SetWindowTextW(context.edit,L"");return false;}
    if(command.original.size()>500){context.notice(hwnd,kickchatText::get(kickchatText::MaximumCharacters));return false;}
    if (isRoleCommand(command.kind)) { prepareRoleCommand(hwnd, context, command); return false; }
    if (command.kind == kickcmd::CHAT && !context.broadcasterId) {
        context.notice(hwnd, kickchatText::get(kickchatText::WaitingChannelId)); return false;
    }
    if ((command.kind == kickcmd::TITLE || command.kind == kickcmd::GAME) &&
        !canManageChannel(context.channel, context.broadcasterId)) {
        context.notice(hwnd, kickchatText::get(kickchatText::OwnChannelOnly)); return false;
    }
    if ((command.kind == kickcmd::BAN || command.kind == kickcmd::TIMEOUT ||
         command.kind == kickcmd::UNBAN) &&
        !canManageChannel(context.channel, context.broadcasterId)) {
        context.notice(hwnd, kickchatText::get(kickchatText::OwnRolesOnly)); return false;
    }
    if (command.kind == kickcmd::USER) {
        if (!kickcmd::validUsername(command.target)) {
            context.notice(hwnd, kickchatText::get(kickchatText::InvalidUserName));
            return false;
        }
        const std::string channel = context.hasChannel ? context.channel : authenticatedChannel();
        ChatRecord person;
        if (!findChatUser(command.target, person, channel)) {
            person.user = utf8FromWide(command.target);
            person.channel = channel;
            if (!authenticatedChannel().empty() &&
                kickcmd::lower(command.target) == toWide(authenticatedChannel()) &&
                authenticatedUserId()) {
                person.id = decimalId(authenticatedUserId());
                person.user = authenticatedChannel();
            } else {
                const std::map<std::wstring, SeenUser> &known =
                    (*context.knownUsers);
                std::map<std::wstring, SeenUser>::const_iterator found =
                    known.find(kickcmd::lower(command.target));
                if (found != known.end() && found->second.id) {
                    person.id = decimalId(found->second.id);
                    person.user = utf8FromWide(found->second.name);
                }
            }
        }
        showUserProfile(hwnd, person);
        SetWindowTextW(context.edit, L"");
        return false;
    }
    if(InterlockedCompareExchange(&g_authenticated,1,1)==0){
        context.notice(hwnd,kickchatText::get(kickchatText::NeedAuthorization));return false;
    }
    if(InterlockedCompareExchange(&g_taskBusy,1,0)!=0)return false;
    PendingSend *pending=new PendingSend();pending->command=command;
    pending->broadcasterId = context.broadcasterId;
    pending->channel = context.channel;
    if(command.kind==kickcmd::BAN||command.kind==kickcmd::TIMEOUT||
       command.kind==kickcmd::UNBAN||command.kind==kickcmd::USER) {
        if(!authenticatedChannel().empty() &&
           kickcmd::lower(command.target)==toWide(authenticatedChannel()) && command.kind!=kickcmd::USER){
            context.notice(hwnd,kickchatText::get(kickchatText::CannotModerateSelf));
            delete pending;InterlockedExchange(&g_taskBusy,0);return false;
        }
        if(!authenticatedChannel().empty() &&
           kickcmd::lower(command.target)==toWide(authenticatedChannel()) && command.kind==kickcmd::USER) {
            // The authenticated broadcaster ID is already verified by OAuth.
            pending->targetId=authenticatedUserId();
            pending->command.target=toWide(authenticatedChannel());
        } else {
            const std::map<std::wstring, SeenUser> &known = (*context.knownUsers);
            std::map<std::wstring,SeenUser>::const_iterator u=known.find(kickcmd::lower(command.target));
            if(u==known.end() || !u->second.id){
                context.notice(hwnd,std::wstring(kickchatText::get(kickchatText::UnknownUserId))+
                               command.target+kickchatText::get(kickchatText::UnknownUserIdSuffix));
                delete pending;InterlockedExchange(&g_taskBusy,0);return false;
            }
            pending->targetId=u->second.id;
            // Show the user's canonical spelling from a real chat event.
            pending->command.target=u->second.name;
        }
    }
    if(command.kind==kickcmd::BAN||command.kind==kickcmd::TIMEOUT){
        std::wstring action=kickchatText::get(command.kind==kickcmd::BAN?
            kickchatText::BanConfirm:kickchatText::TimeoutConfirm);
        std::wstring question=action+pending->command.target+L" (ID "+toWide(decimalId(pending->targetId))+L")?";
        if(MessageBoxW(hwnd,question.c_str(),kickchatText::get(kickchatText::ModerationConfirmTitle),MB_ICONWARNING|MB_YESNO|MB_DEFBUTTON2)!=IDYES){
            delete pending;InterlockedExchange(&g_taskBusy,0);return false;
        }
    }
    HANDLE t=CreateThread(NULL,0,sendWorker,pending,0,NULL);
    if(!t){delete pending;InterlockedExchange(&g_taskBusy,0);context.notice(hwnd,kickchatText::get(kickchatText::SendThreadError));return false;}
    CloseHandle(t);EnableWindow(context.sendButton,FALSE);
    return true;
}
