#include "kickchat_identity.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_oauth.hpp"
#include "kickchat_category_lookup.hpp"
#include "kickchat_json.hpp"
#include "kickchat_api_http.hpp"

static void sendResult(const std::wstring &message, const std::wstring &original,
                       bool ok, const std::string &channel) {
    SendResult *r = new SendResult();
    r->message = message; r->original = original; r->success = ok; r->channel = channel;
    if (!g_window || !PostMessageW(g_window,WM_SENT,0,(LPARAM)r)) delete r;
}
std::string decimalId(unsigned long long n) {
    char buf[32];sprintf(buf,"%llu",n);return buf;
}
DWORD WINAPI sendWorker(LPVOID parameter) {
    PendingSend *pending=(PendingSend*)parameter;
    kickcmd::Command command=pending->command;
    unsigned long long targetId=pending->targetId;
    unsigned long long broadcasterId=pending->broadcasterId;
    const std::string channel=pending->channel;
    delete pending;
    std::wstring error,confirmation;bool success=false;
    const DWORD started=GetTickCount();
    DWORD identityMs=0;
    do {
        if(!authorizeCommand(error))break;
        identityMs=GetTickCount()-started;
        const std::string broadcaster=decimalId(broadcasterId ? broadcasterId : authenticatedUserId());
        if(command.kind==kickcmd::CHAT) {
            std::string utf8=utf8FromWide(command.original);
            if(utf8.empty()||utf8.size()>2048){error=kickchatText::get(kickchatText::SendInvalidUtf8);break;}
            std::string json="{\"type\":\"user\",\"broadcaster_user_id\":"+broadcaster+
                ",\"content\":"+jsonEscape(utf8)+"}";
            HttpResult r=kickAuthorized(L"/public/v1/chat",L"POST",json,error);
            if(r.status!=200){if(error.empty())error=kickchatText::get(kickchatText::SendApiFailed)+apiError(r);break;}
            Json resp;if(!parseJson(r.body,resp)||resp.get("data").get("is_sent").str()!="true"){
                error=kickchatText::get(kickchatText::SendNotConfirmed);break;
            }
            confirmation=kickchatText::get(kickchatText::SendMessageSent);success=true;
        } else if(command.kind==kickcmd::TITLE) {
            if(!canManageChannel(channel,broadcasterId)) {
                error=kickchatText::get(kickchatText::OwnChannelOnly);break;
            }
            std::string title=utf8FromWide(command.value);
            if(title.empty()||title.size()>500){error=kickchatText::get(kickchatText::SendTitleInvalid);break;}
            HttpResult r=kickAuthorized(L"/public/v1/channels",L"PATCH",
                "{\"stream_title\":"+jsonEscape(title)+"}",error);
            if(r.status!=204){if(error.empty())error=kickchatText::get(kickchatText::SendTitleError)+apiError(r);break;}
            confirmation=kickchatText::get(kickchatText::SendTitleChanged)+command.value;success=true;
        } else if(command.kind==kickcmd::GAME) {
            if(!canManageChannel(channel,broadcasterId)) {
                error=kickchatText::get(kickchatText::OwnChannelOnly);break;
            }
            std::string categoryId;
            std::wstring categoryName;
            if(!lookupKickCategory(command.value,categoryId,categoryName,error))break;
            HttpResult r=kickAuthorized(L"/public/v1/channels",L"PATCH",
                "{\"category_id\":"+categoryId+"}",error);
            if(r.status!=204 || !r.error.empty()) {
                if(error.empty())error=kickchatText::get(kickchatText::SendCategoryError)+apiError(r);
                break;
            }
            confirmation=kickchatText::get(kickchatText::SendCategoryChanged)+categoryName;success=true;
        } else if(command.kind==kickcmd::BAN || command.kind==kickcmd::TIMEOUT || command.kind==kickcmd::UNBAN) {
            if(!canManageChannel(channel,broadcasterId)) {
                error=kickchatText::get(kickchatText::OwnRolesOnly);break;
            }
            if(targetId==authenticatedUserId()) {
                error=kickchatText::get(kickchatText::CannotModerateSelf);break;
            }
            if(!targetId){error=kickchatText::get(kickchatText::SendUnknownUser);break;}
            std::string json="{\"broadcaster_user_id\":"+broadcaster+
                ",\"user_id\":"+decimalId(targetId);
            if(command.kind==kickcmd::TIMEOUT)json+=",\"duration\":"+decimalId(command.minutes);
            if(command.kind!=kickcmd::UNBAN && !command.value.empty()) {
                std::string reason=utf8FromWide(command.value);
                if(reason.empty()||reason.size()>100){error=kickchatText::get(kickchatText::SendInvalidReason);break;}
                json+=",\"reason\":"+jsonEscape(reason);
            }
            json+="}";
            HttpResult r=kickAuthorized(L"/public/v1/moderation/bans",
                command.kind==kickcmd::UNBAN?L"DELETE":L"POST",json,error);
            if(r.status!=200){if(error.empty())error=kickchatText::get(kickchatText::SendModerationError)+apiError(r);break;}
            if(command.kind==kickcmd::BAN)confirmation=kickchatText::get(kickchatText::SendBanned)+command.target;
            else if(command.kind==kickcmd::UNBAN)confirmation=kickchatText::get(kickchatText::SendUnbanned)+command.target;
            else confirmation=kickchatText::get(kickchatText::SendTimeout)+command.target+
                kickchatText::get(kickchatText::SendTimeoutDuration)+
                toWide(decimalId(command.minutes))+L" min";
            success=true;
        } else if(command.kind==kickcmd::USER) {
            if(!targetId){error=kickchatText::get(kickchatText::SendUserMissing);break;}
            // The Pusher sender ID is a genuine Kick numeric ID. Some Kick API
            // installations only expose the OAuth owner's details via /users;
            // keep the ID and username available even when that API is limited.
            confirmation=kickchatText::get(kickchatText::SendUserPrefix)+command.target+L" | ID: "+toWide(decimalId(targetId));
            std::wstring path=L"/public/v1/users?id="+toWide(decimalId(targetId));
            HttpResult r=kickAuthorized(path.c_str(),L"GET","",error);
            if(r.status==200) {
                Json resp;
                if(parseJson(r.body,resp)) {
                    const Json &users=resp.get("data");
                    if(users.type==Json::ARRAY && !users.arr.empty()) {
                        const Json &user=users.arr[0];
                        if(user.get("user_id").str()==decimalId(targetId)) {
                            std::string name=user.get("name").str();
                            if(!name.empty())confirmation=kickchatText::get(kickchatText::SendUserPrefix)+toWide(name)+L" | ID: "+toWide(decimalId(targetId));
                        }
                    }
                }
            }
            error.clear();success=true;
        } else error=kickchatText::get(kickchatText::SendUnknownCommand);
    } while(false);
    if(!success){
        if(error.empty())error=kickchatText::get(kickchatText::SendUnknownError);
        if(error.find(L"HTTP 403")!=std::wstring::npos)error+=kickchatText::get(kickchatText::SendRefreshPermissions);
        writeApiLog(std::wstring(L"Comando ")+(command.kind==kickcmd::CHAT?L"chat":
                    command.kind==kickcmd::GAME?L"categoria":L"moderacion")+L": "+error);
    }
    if(command.kind==kickcmd::CHAT) {
        DWORD elapsed=GetTickCount()-started;
        if(elapsed>=1200) {
            writeApiLog(L"Chat timing: identity="+toWide(decimalId(identityMs))+
                        L"ms total="+toWide(decimalId(elapsed))+L"ms");
        }
    }
    sendResult(success?confirmation:error,command.original,success,channel);
    InterlockedExchange(&g_taskBusy,0);
    return 0;
}
