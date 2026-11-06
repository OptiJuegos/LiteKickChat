#include "kickchat_identity.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_oauth.hpp"
#include "kickchat_json.hpp"
#include "kickchat_credentials.hpp"
#include "kickchat_api_http.hpp"
#include "kickchat_oauth_callback.hpp"
#include <cctype>

static const wchar_t *REDIRECT_URI = L"http://localhost:17839/callback";
static const char *REDIRECT_UTF8 = "http://localhost:17839/callback";
volatile LONG g_taskBusy = 0;
volatile LONG g_authenticated = 0;
volatile LONG g_oauthPending = 0;
volatile LONG g_oauthCancelRequested = 0;
struct TokenData {
    std::string access, refresh;
    unsigned long long expires, userId;
    TokenData() : expires(0), userId(0) {}
};
static TokenData g_token;
static bool g_accountVerified = false;
static SRWLOCK g_channelLock = SRWLOCK_INIT;
static std::string g_verifiedChannel;

static void setAuthenticatedChannel(const std::string &channel) {
    AcquireSRWLockExclusive(&g_channelLock);
    g_verifiedChannel = channel;
    ReleaseSRWLockExclusive(&g_channelLock);
}

std::string authenticatedChannel() {
    AcquireSRWLockShared(&g_channelLock);
    const std::string channel = g_verifiedChannel;
    ReleaseSRWLockShared(&g_channelLock);
    return channel;
}

bool hasCachedToken() { return !g_token.access.empty(); }
void resetAuthSession() {
    g_accountVerified = false;
    setAuthenticatedChannel("");
    if (!g_token.access.empty()) SecureZeroMemory(&g_token.access[0], g_token.access.size());
    if (!g_token.refresh.empty()) SecureZeroMemory(&g_token.refresh[0], g_token.refresh.size());
    g_token = TokenData();
    InterlockedExchange(&g_authenticated, 0);
    DeleteFileW(exeFile(L"kickchat.token").c_str());
}

unsigned long long authenticatedUserId() { return g_token.userId; }

void writeApiLog(const std::wstring &entry) {
    std::wstring path;
    wchar_t full[MAX_PATH]={0};GetModuleFileNameW(NULL,full,MAX_PATH);
    path=full;size_t last=path.find_last_of(L"\\/");
    path=(last==std::wstring::npos?L"":path.substr(0,last+1))+L"kickchat.log";
    HANDLE h=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE)return;
    SYSTEMTIME t;GetLocalTime(&t);char head[64];
    int n=sprintf(head,"%04u-%02u-%02u %02u:%02u:%02u [API] ",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
    int size=WideCharToMultiByte(CP_UTF8,0,entry.c_str(),(int)entry.size(),NULL,0,NULL,NULL);
    std::string line(head,n);
    if(size>0){size_t at=line.size();line.resize(at+size);WideCharToMultiByte(CP_UTF8,0,entry.c_str(),(int)entry.size(),&line[at],size,NULL,NULL);}
    line+="\r\n";DWORD wrote=0;WriteFile(h,line.data(),(DWORD)line.size(),&wrote,NULL);CloseHandle(h);
}
static void sendUi(UINT code,const std::wstring &msg) { UiPacket *p=new UiPacket();p->status=msg;uiPost(code,p); }
static bool readSmallFile(const std::wstring &path,std::string &out,size_t limit) {
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) return false;
    DWORD len=GetFileSize(h,NULL);if(len==INVALID_FILE_SIZE || len>limit){CloseHandle(h);return false;}
    out.resize(len);DWORD got=0;bool ok=!len||ReadFile(h,&out[0],len,&got,NULL)!=0;
    CloseHandle(h);return ok&&got==len;
}
static bool writeSmallFile(const std::wstring &path,const std::string &str) {
    HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE) return false;
    DWORD done=0;bool ok=WriteFile(h,str.data(),(DWORD)str.size(),&done,NULL)!=0;
    CloseHandle(h);return ok&&done==str.size();
}
static std::string shaChallenge(const std::string &verifier) {
    HCRYPTPROV p=0;HCRYPTHASH h=0;BYTE hash[32];DWORD n=32;
    if(!CryptAcquireContextW(&p,NULL,NULL,PROV_RSA_AES,CRYPT_VERIFYCONTEXT))return "";
    bool ok=CryptCreateHash(p,CALG_SHA_256,0,0,&h)!=0;
    if(ok)ok=CryptHashData(h,(const BYTE*)verifier.data(),(DWORD)verifier.size(),0)!=0;
    if(ok)ok=CryptGetHashParam(h,HP_HASHVAL,hash,&n,0)!=0 && n==32;
    if(h)CryptDestroyHash(h);CryptReleaseContext(p,0);
    if(!ok)return "";
    std::string result=base64(hash,n);
    for(size_t i=0;i<result.size();i++){if(result[i]=='+')result[i]='-';if(result[i]=='/')result[i]='_';}
    while(!result.empty()&&result[result.size()-1]=='=')result.resize(result.size()-1);
    return result;
}
static std::string randomHex(int count) {
    if(count<=0||count>64)return "";BYTE bytes[64];if(!randomBytes(bytes,(DWORD)count))return "";
    static const char*hex="0123456789abcdef";std::string result;
    for(int i=0;i<count;i++){result+=hex[bytes[i]>>4];result+=hex[bytes[i]&15];}
    SecureZeroMemory(bytes,sizeof(bytes));return result;
}
static unsigned long long nowSeconds() {
    FILETIME f;GetSystemTimeAsFileTime(&f);ULARGE_INTEGER x;
    x.LowPart=f.dwLowDateTime;x.HighPart=f.dwHighDateTime;
    return (x.QuadPart-116444736000000000ULL)/10000000ULL;
}
static void storeTokens() {
    if(g_token.access.empty()||g_token.refresh.empty())return;
    char buf[96];sprintf(buf,"%llu %llu\n",g_token.expires,g_token.userId);
    std::string clear=g_token.access+"\n"+g_token.refresh+"\n"+buf;
    DATA_BLOB source={(DWORD)clear.size(),(BYTE*)clear.data()},dest={0,NULL};
    if(CryptProtectData(&source,kickchatIdentity::oauthDescription,NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&dest)){
        std::string blob((char*)dest.pbData,dest.cbData);
        writeSmallFile(exeFile(L"kickchat.token"),blob);
        LocalFree(dest.pbData);
    }
    SecureZeroMemory(&clear[0],clear.size());
}
void loadTokens() {
    g_accountVerified = false;
    setAuthenticatedChannel("");
    std::string blob;if(!readSmallFile(exeFile(L"kickchat.token"),blob,32768)||blob.empty())return;
    DATA_BLOB source={(DWORD)blob.size(),(BYTE*)blob.data()},dest={0,NULL};
    if(!CryptUnprotectData(&source,NULL,NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&dest))return;
    std::string plain((char*)dest.pbData,dest.cbData);
    SecureZeroMemory(dest.pbData,dest.cbData);LocalFree(dest.pbData);
    size_t a=plain.find('\n'),b=a==std::string::npos?std::string::npos:plain.find('\n',a+1);
    if(b!=std::string::npos){
        g_token.access=plain.substr(0,a);g_token.refresh=plain.substr(a+1,b-a-1);
        unsigned long long exp=0,uid=0;
        if(sscanf(plain.c_str()+b+1,"%llu %llu",&exp,&uid)==2){g_token.expires=exp;g_token.userId=uid;}
        if(g_token.access.size()>8192||g_token.refresh.size()>8192)g_token=TokenData();
    }
    if(!plain.empty())SecureZeroMemory(&plain[0],plain.size());
}
static bool updateToken(const HttpResult &res) {
    Json root;if(res.status!=200 || !parseJson(res.body,root))return false;
    std::string access=root.get("access_token").str(),refresh=root.get("refresh_token").str();
    if(access.empty()||access.size()>8192)return false;
    std::string scope=root.get("scope").str();
    if(!scope.empty()&&scope.find("chat:write")==std::string::npos)return false;
    if(!refresh.empty())g_token.refresh=refresh;
    g_accountVerified = false;
    setAuthenticatedChannel("");
    g_token.access=access;unsigned lifetime=(unsigned)strtoul(root.get("expires_in").str().c_str(),NULL,10);
    if(lifetime<60||lifetime>86400)lifetime=3600;
    g_token.expires=nowSeconds()+lifetime;
    storeTokens();return true;
}
static bool refreshToken(std::wstring &error) {
    std::string id,secret;if(!loadCredentials(id,secret)){error=kickchatText::get(kickchatText::AuthMissingCredentials);return false;}
    if(g_token.refresh.empty()){error=kickchatText::get(kickchatText::AuthLoginRequired);return false;}
    std::string form="grant_type=refresh_token&client_id="+formEncode(id)+"&client_secret="+formEncode(secret)+
        "&refresh_token="+formEncode(g_token.refresh);
    HttpResult r=kickHttp(L"id.kick.com",L"/oauth/token",L"POST",form,"",true);
    if (!form.empty()) SecureZeroMemory(&form[0], form.size());
    if (!secret.empty()) SecureZeroMemory(&secret[0], secret.size());
    if(!updateToken(r)){error=kickchatText::get(kickchatText::AuthRefreshError)+apiError(r);InterlockedExchange(&g_authenticated,0);return false;}
    return true;
}
static bool ensureToken(std::wstring &error) {
    if(g_token.access.empty()||g_token.expires<=nowSeconds()+90){
        if(!refreshToken(error))return false;
    }return true;
}
static bool verifyAccount(std::wstring &error) {
    if(!ensureToken(error))return false;
    HttpResult r=kickHttp(L"api.kick.com",L"/public/v1/users",L"GET","",g_token.access,false);
    if(r.status==401 && refreshToken(error))r=kickHttp(L"api.kick.com",L"/public/v1/users",L"GET","",g_token.access,false);
    if(r.status!=200){error=kickchatText::get(kickchatText::AuthVerifyError)+apiError(r);return false;}
    Json root;if(!parseJson(r.body,root)){error=kickchatText::get(kickchatText::AuthInvalidUserJson);return false;}
    Json user=root.get("data");if(user.type==Json::ARRAY&&!user.arr.empty())user=user.arr[0];
    std::string name=user.get("name").str();if(name.empty())name=user.get("username").str();
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return (char)std::tolower(c);});
    if (!kickcmd::validUsername(toWide(name))) {
        error = kickchatText::get(kickchatText::InvalidUserName); return false;
    }
    std::string id=user.get("user_id").str();if(id.empty())id=user.get("id").str();
    char*end=NULL;unsigned long long parsed=strtoull(id.c_str(),&end,10);
    if(id.empty()||!end||*end||!parsed){error=kickchatText::get(kickchatText::AuthMissingBroadcasterId);return false;}
    g_token.userId=parsed;storeTokens();setAuthenticatedChannel(name);g_accountVerified = true;
    InterlockedExchange(&g_authenticated,1);return true;
}
DWORD WINAPI oauthWorker(LPVOID) {
    std::wstring error;std::string id,secret;
    bool tokenChanged=false;
    do {
        if(InterlockedCompareExchange(&g_oauthCancelRequested,0,0)!=0){error=kickchatText::get(kickchatText::AuthCancelled);break;}
        if(!loadCredentials(id,secret)){error=kickchatText::get(kickchatText::AuthMissingCredentials);break;}
        WSADATA w;if(WSAStartup(MAKEWORD(2,2),&w)!=0){error=kickchatText::get(kickchatText::AuthWinsockUnavailable);break;}
        SOCKET listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(listener==INVALID_SOCKET){error=kickchatText::get(kickchatText::AuthCallbackSocketFailed);WSACleanup();break;}
        sockaddr_in address={0};address.sin_family=AF_INET;address.sin_port=htons(17839);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(bind(listener,(sockaddr*)&address,sizeof(address))!=0||listen(listener,4)!=0){
            error=kickchatText::get(kickchatText::AuthPortUnavailable);
            closesocket(listener);WSACleanup();break;
        }
        std::string verifier=randomHex(32),state=randomHex(16),challenge=shaChallenge(verifier);
        if(challenge.empty()||state.empty()){error=kickchatText::get(kickchatText::AuthPkceError);closesocket(listener);WSACleanup();break;}
        if(InterlockedCompareExchange(&g_oauthCancelRequested,0,0)!=0){
            error=kickchatText::get(kickchatText::AuthCancelled);closesocket(listener);WSACleanup();break;
        }
        std::string url="https://id.kick.com/oauth/authorize?response_type=code&client_id="+formEncode(id)+
            "&redirect_uri="+formEncode(REDIRECT_UTF8)+"&state="+formEncode(state)+
            "&scope="+formEncode("user:read chat:write channel:write moderation:ban")+"&code_challenge="+formEncode(challenge)+
            "&code_challenge_method=S256";
        sendUi(WM_AUTH_CHANGED,kickchatText::get(kickchatText::AuthBrowserPrefix));
        HINSTANCE opened=ShellExecuteW(NULL,L"open",toWide(url).c_str(),NULL,NULL,SW_SHOWNORMAL);
        if((INT_PTR)opened<=32){error=kickchatText::get(kickchatText::AuthBrowserFailed);closesocket(listener);WSACleanup();break;}
        std::string code;bool cb=receiveOAuthCallback(listener,state,code,error);
        closesocket(listener);WSACleanup();if(!cb)break;
        InterlockedExchange(&g_oauthPending,2);
        if(InterlockedCompareExchange(&g_oauthCancelRequested,0,0)!=0){error=kickchatText::get(kickchatText::AuthCancelled);break;}
        sendUi(WM_AUTH_CHANGED,kickchatText::get(kickchatText::AuthExchangingCode));
        std::string form="grant_type=authorization_code&client_id="+formEncode(id)+"&client_secret="+
            formEncode(secret)+"&redirect_uri="+formEncode(REDIRECT_UTF8)+
            "&code_verifier="+formEncode(verifier)+"&code="+formEncode(code);
        SecureZeroMemory(&code[0],code.size());
        HttpResult r=kickHttp(L"id.kick.com",L"/oauth/token",L"POST",form,"",true);
        if (!form.empty()) SecureZeroMemory(&form[0], form.size());
        if (!secret.empty()) SecureZeroMemory(&secret[0], secret.size());
        if(!updateToken(r)){error=L"OAuth token: "+apiError(r);break;}
        tokenChanged=true;
        if(!verifyAccount(error))break;
        sendUi(WM_AUTH_CHANGED,std::wstring(kickchatText::get(kickchatText::Authorized))+
               toWide(authenticatedChannel()));
    } while(false);
    if(!secret.empty()) SecureZeroMemory(&secret[0], secret.size());
    if(!error.empty()){
        if(tokenChanged) InterlockedExchange(&g_authenticated,0);
        writeApiLog(L"OAuth: "+error);
    }
    InterlockedExchange(&g_oauthPending,0);
    InterlockedExchange(&g_taskBusy,0);
    sendUi(WM_AUTH_CHANGED,error.empty()?
           std::wstring(kickchatText::get(kickchatText::Authorized))+
           toWide(authenticatedChannel()):error);
    return 0;
}
// Every operation has one worker: tokens never cross threads concurrently.
bool authorizeCommand(std::wstring &error) {
    return ensureToken(error) && (g_accountVerified || verifyAccount(error));
}

HttpResult kickAuthorized(const wchar_t *path,const wchar_t *method,const std::string &body,
                          std::wstring &error) {
    HttpResult r=kickHttp(L"api.kick.com",path,method,body,g_token.access,false);
    if(r.status==401 && refreshToken(error)) {
        if (!verifyAccount(error)) return r;
        r=kickHttp(L"api.kick.com",path,method,body,g_token.access,false);
    }
    return r;
}
