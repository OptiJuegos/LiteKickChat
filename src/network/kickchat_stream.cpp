#include "kickchat_identity.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_platform.hpp"
#include "kickchat_json.hpp"
#include "kickchat_websocket.hpp"
#ifdef KICKCHAT_USE_CURL
#include "kickchat_tls_curl.hpp"
typedef CurlTlsTransport PusherTls;
#else
#include "kickchat_tls_schannel.hpp"
typedef Tls PusherTls;
#endif
#include "kickchat_http.hpp"
#include "kickchat_stream.hpp"
#include "kickchat_recent.hpp"

static std::string g_pusherKey = "34bf7a0ff419a2a775b9";
static std::string g_wsHost = "ws-us2.pusher.com";
static std::wstring g_wsHostWide = L"ws-us2.pusher.com";

static unsigned chatId(const Json &obj) {
    std::string id=obj.get("id").str();
    if (id.empty()) return 0;
    for (size_t k=0;k<id.size();++k) if (id[k]<'0'||id[k]>'9') return 0;
    unsigned long n=strtoul(id.c_str(),NULL,10);
    return n && n<=0x7FFFFFFFul ? (unsigned)n : 0;
}
static std::string fixEmotes(const std::string &s) {
    // Legacy text fallback kept until its callers are reviewed.
    std::string out; out.reserve(s.size());
    for (size_t i=0;i<s.size();) {
        if (s.compare(i,7,"[emote:")==0) {
            size_t end=s.find(']',i+7), colon=s.find(':',i+7);
            if (end!=std::string::npos && end-i<128 && colon!=std::string::npos && colon<end) {
                out+=':';out+=s.substr(colon+1,end-colon-1);out+=':';i=end+1;continue;
            }
        }
        out+=s[i++];
    }
    return out;
}
// Retained original declaration; do not remove without checking callers.
static void processEvent(const std::string &frame, bool &subscribed, unsigned id);

static std::wstring cachePath() {
    wchar_t path[MAX_PATH]={0};
    GetModuleFileNameW(NULL,path,MAX_PATH);
    std::wstring name=path;
    size_t at=name.find_last_of(L"\\/");
    return (at==std::wstring::npos ? L"" : name.substr(0,at+1)) + L"kickchat.id";
}
static unsigned getCachedId() {
    std::wstring path=cachePath();
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (f==INVALID_HANDLE_VALUE) return 0;
    char buf[64]={0}; DWORD n=0;
    bool ok=ReadFile(f,buf,sizeof(buf)-1,&n,NULL)!=0;
    CloseHandle(f);
    if (!ok || n==0) return 0;
    unsigned long id=strtoul(buf,NULL,10);
    return id && id<=0x7FFFFFFFul ? (unsigned)id : 0;
}
static void putCachedId(unsigned id) {
    HANDLE f=CreateFileW(cachePath().c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (f==INVALID_HANDLE_VALUE) return;
    char buf[32]; int n=sprintf(buf,"%u\r\n",id); DWORD written=0;
    WriteFile(f,buf,n,&written,NULL); CloseHandle(f);
}
// Optional kickchat.pusher next to the EXE: public_key [cluster].
// Example: 34bf7a0ff419a2a775b9 us2.
// A changed Pusher key or cluster does not require recompilation.
void loadPusherConfig() {
    wchar_t module[MAX_PATH] = {0};
    if (!GetModuleFileNameW(NULL, module, MAX_PATH)) return;
    std::wstring path(module);
    size_t pos = path.find_last_of(L"\\/");
    path = (pos == std::wstring::npos ? L"" : path.substr(0, pos+1)) + L"kickchat.pusher";
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    char buf[128] = {0}; DWORD n = 0;
    bool ok = ReadFile(f, buf, sizeof(buf)-1, &n, NULL) != 0;
    CloseHandle(f);
    if (!ok || !n) return;
    std::string line(buf, n);
    std::string key, cluster;
    size_t a = line.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return;
    size_t b = line.find_first_of(" \t\r\n", a);
    key = line.substr(a, b == std::string::npos ? std::string::npos : b-a);
    if (b != std::string::npos) {
        size_t c = line.find_first_not_of(" \t\r\n", b);
        if (c != std::string::npos) {
            size_t d = line.find_first_of(" \t\r\n", c);
            cluster = line.substr(c, d == std::string::npos ? std::string::npos : d-c);
        }
    }
    // Reject arbitrary characters before constructing the WebSocket host or path.
    bool validKey = key.size() >= 12 && key.size() <= 64;
    for (size_t i=0; i<key.size(); ++i)
        if (!((key[i]>='0' && key[i]<='9') ||
              (key[i]>='a' && key[i]<='z') ||
              (key[i]>='A' && key[i]<='Z'))) validKey=false;
    bool validCluster = cluster.empty() || (cluster.size()>=2 && cluster.size()<=12);
    for (size_t i=0; i<cluster.size(); ++i)
        if (!((cluster[i]>='0' && cluster[i]<='9') ||
              (cluster[i]>='a' && cluster[i]<='z') || cluster[i]=='-')) validCluster=false;
    if (!validKey || !validCluster) {
        recordError(L"Configuracion", kickchatText::get(kickchatText::NetworkPusherInvalid));
        return;
    }
    g_pusherKey = key;
    if (!cluster.empty()) {
        g_wsHost = "ws-" + cluster + ".pusher.com";
        g_wsHostWide = toWide(g_wsHost);
    }
}

static unsigned lookupId(const std::string &channel, unsigned long long &broadcasterId) {
    const std::wstring slug = toWide(channel);
    const std::wstring urls[] = {
        L"https://kick.com/api/v2/channels/" + slug + L"/chatroom",
        L"https://kick.com/api/v2/channels/" + slug,
        L"https://kick.com/api/v1/channels/" + slug
    };
    for (int k=0;k<3 && running();k++) {
        std::string text; Json data;
        if (httpGet(urls[k], text, 1024 * 1024) && parseJson(text,data)) {
            unsigned id=0;
            if (k==0) id=chatId(data);
            else id=chatId(data.get("chatroom"));
            const std::string streamer = data.get("user_id").str();
            if (!streamer.empty()) broadcasterId = strtoull(streamer.c_str(), NULL, 10);
            if (!broadcasterId) broadcasterId = strtoull(data.get("user").get("id").str().c_str(), NULL, 10);
            if (id) return id;
        }
    }
    return 0;
}

struct RecentRequest {
    std::string channel;
    unsigned chatroomId;
};

static DWORD WINAPI recentWorker(LPVOID parameter) {
    RecentRequest *request = static_cast<RecentRequest *>(parameter);
    fetchRecentKickMessages(request->channel, request->chatroomId);
    delete request;
    return 0;
}

static void fetchHistoryAsync(const std::string &channel, unsigned chatroomId) {
    RecentRequest *request = new RecentRequest();
    request->channel = channel;
    request->chatroomId = chatroomId;
    HANDLE thread = CreateThread(NULL, 0, recentWorker, request, 0, NULL);
    if (thread) CloseHandle(thread);
    else delete request;
}

static std::string stringifyUnsigned(unsigned id) {char s[32];sprintf(s,"%u",id);return s;}
static bool handleJson(WebSocket &ws,const std::string &json,unsigned id,const std::string &channel) {
    Json msg;if (!parseJson(json,msg)) return true;
    std::string event=msg.get("event").str();
    if (event=="pusher:connection_established") {
        std::string chan="chatrooms."+stringifyUnsigned(id)+".v2";
        std::string request="{\"event\":\"pusher:subscribe\",\"data\":{\"auth\":\"\",\"channel\":\""+chan+"\"}}";
    if (!ws.sendText(request)) {recordError(L"Pusher",kickchatText::get(kickchatText::NetworkSubscribeFailed));return false;}
        return true;
    }
    if (event=="pusher:ping") return ws.sendText("{\"event\":\"pusher:pong\",\"data\":{}}");
    if (event=="pusher_internal:subscription_succeeded") {
        setChannelStatus(channel,kickchatText::get(kickchatText::NetworkConnectedTo)+toWide(channel));return true;
    }
    if (event=="pusher:subscription_error" || event=="pusher:error") {
        Json details = msg.get("data");
        if (details.type==Json::STRING) {
            Json nested;
            if (parseJson(details.str(),nested)) details=nested;
        }
        std::string reason, code;
        if (details.type==Json::OBJECT) {
            reason = details.get("message").str();
            code = details.get("code").str();
            if (reason.empty()) reason=details.get("error").str();
        } else reason=details.str();
        if (reason.empty()) reason="Error Pusher sin mensaje";
        if (reason.size()>240) reason.resize(240);
        if (!code.empty()) reason += " (codigo " + code.substr(0,24) + ")";
        if (code=="4001") reason += "; comprobar clave publica/cluster Pusher";
        recordError(L"Pusher",toWide(reason));return false;
    }
    if (event.find("ChatMessage")==std::string::npos) return true;
    Json d=msg.get("data");
    if (d.type==Json::STRING) {Json nested;if (!parseJson(d.str(),nested)) return true;d=nested;}
    if (d.type!=Json::OBJECT) return true;
    ChatRecord record;
    if (parseKickChatRecord(d, record)) {
        addChatRecord(record, chatColor(record.color), channel);
    }
    return true;
}
bool startChannelSession(ChannelSession *session) {
    if (!session) return false;
    session->thread = CreateThread(NULL, 0, networkThread, session, 0, NULL);
    return session->thread != NULL;
}

void stopChannelSession(ChannelSession *session) {
    if (session) InterlockedExchange(&session->active, 0);
}

DWORD WINAPI networkThread(LPVOID parameter) {
    ChannelSession *session = static_cast<ChannelSession *>(parameter);
    const std::string channel = session->slug;
    WSADATA data;
    int startup = WSAStartup(MAKEWORD(2, 2), &data);
    if (startup != 0) {
        setChannelStatus(channel, L"Winsock: " + toWide(std::to_string(startup)));
        return 0;
    }
    unsigned id = session->chatroomId;
    unsigned long long broadcasterId = 0;
    // Legacy kickchat.id has no channel slug, so it cannot safely identify a public user's chat.
    if (!id && InterlockedCompareExchange(&session->active, 0, 0)) {
    setChannelStatus(channel, kickchatText::get(kickchatText::NetworkLookingFor) + toWide(channel) + L"...");
        id = lookupId(channel, broadcasterId);
    }
    if (!broadcasterId && InterlockedCompareExchange(&session->active, 0, 0)) {
        std::string result;
        Json user;
        if (httpGet(L"https://kick.com/api/v1/users/" + toWide(channel), result, 1024 * 1024) &&
            parseJson(result, user)) {
            broadcasterId = strtoull(user.get("id").str().c_str(), NULL, 10);
        }
    }
    if (broadcasterId) setChannelBroadcaster(channel, broadcasterId);
    if (!id) {
        setChannelStatus(channel, kickchatText::get(kickchatText::NetworkIdMissing) + toWide(channel));
        WSACleanup();
        return 0;
    }
    fetchHistoryAsync(channel, id);
    unsigned attempt = 0;
    while (running() && InterlockedCompareExchange(&session->active, 0, 0)) {
    setChannelStatus(channel, kickchatText::get(kickchatText::NetworkConnectingTo) + toWide(channel) + L"...");
        {
            PusherTls tls;
            WebSocket ws(tls);
            if (ws.open(g_wsHost, g_wsHostWide, g_pusherKey)) {
                attempt = 0;
                DWORD lastPing = GetTickCount();
                while (running() && InterlockedCompareExchange(&session->active, 0, 0)) {
                    std::string message;
                    int result = ws.nextText(message);
                    if (result < 0) break;
                    if (result == 1 && !handleJson(ws, message, id, channel)) break;
                    if ((DWORD)(GetTickCount() - lastPing) > 30000) {
                        if (!ws.sendText("{\"event\":\"pusher:ping\",\"data\":{}}")) break;
                        lastPing = GetTickCount();
                    }
                }
            }
        }
        if (!running() || !InterlockedCompareExchange(&session->active, 0, 0)) break;
        unsigned delay = std::min(30u, 1u << std::min(attempt++, 5u));
        setChannelStatus(channel, kickchatText::get(kickchatText::NetworkReconnecting) + toWide(channel) +
                          kickchatText::get(kickchatText::NetworkReconnectIn) +
                          toWide(std::to_string(delay)) + L"s");
        for (unsigned i = 0; i < delay * 10 && running() &&
             InterlockedCompareExchange(&session->active, 0, 0); ++i) Sleep(100);
    }
    WSACleanup();
    return 0;
}
