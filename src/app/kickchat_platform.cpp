#include "kickchat_platform.hpp"
#include "kickchat_strings.hpp"

volatile LONG g_running = 1;
HWND g_window = NULL;
std::wstring g_lastError;
static SRWLOCK g_logLock = SRWLOCK_INIT;

bool running() { return InterlockedCompareExchange(&g_running, 1, 1) != 0; }
void uiPost(UINT msg, UiPacket *p) {
    if (!g_window || !PostMessageW(g_window, msg, 0, (LPARAM)p)) delete p;
}
void setStatus(const std::wstring &s) {
    UiPacket *p = new UiPacket(); p->status = s; uiPost(WM_STATUS, p);
}
void setChannelStatus(const std::string &channel, const std::wstring &status) {
    UiPacket *p = new UiPacket(); p->channel = channel; p->status = status; uiPost(WM_STATUS, p);
}
void setChannelBroadcaster(const std::string &channel, unsigned long long userId) {
    UiPacket *p = new UiPacket(); p->channel = channel; p->broadcasterId = userId;
    uiPost(WM_CHANNEL_ID, p);
}
std::wstring toWide(const std::string &s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), NULL, 0);
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (len <= 0) { flags = 0; len = MultiByteToWideChar(CP_UTF8, flags, s.data(), (int)s.size(), NULL, 0); }
    if (len <= 0) return kickchatText::get(kickchatText::InvalidUtf8);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, flags, s.data(), (int)s.size(), &out[0], len);
    return out;
}
// Keep transport errors visible and record their original status codes.
static std::wstring numericError(DWORD n) {
    wchar_t buf[40]; swprintf(buf,40,L"0x%08lX",(unsigned long)n);
    return buf;
}
void recordError(const wchar_t *stage, const std::wstring &cause) {
    const std::wstring error = std::wstring(stage) + L": " + cause;
    AcquireSRWLockExclusive(&g_logLock);
    g_lastError = error;
    setStatus(kickchatText::get(kickchatText::ErrorPrefix) + error);
    wchar_t path[MAX_PATH]={0};
    GetModuleFileNameW(NULL,path,MAX_PATH);
    std::wstring target(path);
    size_t pos=target.find_last_of(L"\\/");
    target=(pos==std::wstring::npos ? L"" : target.substr(0,pos+1))+L"kickchat.log";
    HANDLE h=CreateFileW(target.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (h==INVALID_HANDLE_VALUE) { ReleaseSRWLockExclusive(&g_logLock); return; }
    SYSTEMTIME st; GetLocalTime(&st);
    char prefix[80];
    int n=sprintf(prefix,"%04u-%02u-%02u %02u:%02u:%02u ",
                  (unsigned)st.wYear,(unsigned)st.wMonth,(unsigned)st.wDay,
                  (unsigned)st.wHour,(unsigned)st.wMinute,(unsigned)st.wSecond);
    int bytes=WideCharToMultiByte(CP_UTF8,0,error.c_str(),(int)error.size(),NULL,0,NULL,NULL);
    std::string line(prefix,(size_t)n);
    if (bytes>0) {
        size_t at=line.size();line.resize(at+(size_t)bytes);
        WideCharToMultiByte(CP_UTF8,0,error.c_str(),(int)error.size(),&line[at],bytes,NULL,NULL);
    }
    line+="\r\n";
    DWORD wrote=0; WriteFile(h,line.data(),(DWORD)line.size(),&wrote,NULL);
    CloseHandle(h);
    ReleaseSRWLockExclusive(&g_logLock);
}
void recordCode(const wchar_t *stage,DWORD error) { recordError(stage,numericError(error)); }

void addChat(const std::string &user, const std::string &content, COLORREF col,
             unsigned long long userId, const std::string &channel) {
    UiPacket *p = new UiPacket();
    p->chat.user = toWide(user.empty() ? "Usuario" : user);
    p->chat.content = toWide(content); p->chat.color = col; p->chat.userId=userId;
    p->chat.channel = channel;
    uiPost(WM_CHAT, p);
}

COLORREF chatColor(const std::string &value) {
    if (value.size() != 7 || value[0] != '#') return RGB(75, 220, 125);
    for (size_t i = 1; i < value.size(); ++i)
        if (!((value[i] >= '0' && value[i] <= '9') ||
              (value[i] >= 'a' && value[i] <= 'f') ||
              (value[i] >= 'A' && value[i] <= 'F'))) return RGB(75, 220, 125);
    unsigned hex = static_cast<unsigned>(strtoul(value.c_str() + 1, NULL, 16));
    return RGB((hex >> 16) & 255, (hex >> 8) & 255, hex & 255);
}

void addChatRecord(const ChatRecord &record, COLORREF color, const std::string &channel,
                   bool backfill) {
    UiPacket *packet = new UiPacket();
    packet->chat.user = toWide(record.user);
    packet->chat.content = toWide(record.content);
    packet->chat.color = color;
    packet->chat.userId = strtoull(record.id.c_str(), NULL, 10);
    packet->chat.record = record;
    packet->chat.channel = channel;
    packet->chat.record.channel = channel;
    packet->chat.backfill = backfill;
    SYSTEMTIME time;
    GetLocalTime(&time);
    char timestamp[32];
    sprintf(timestamp, "%04u-%02u-%02u %02u:%02u:%02u",
            (unsigned)time.wYear, (unsigned)time.wMonth, (unsigned)time.wDay,
            (unsigned)time.wHour, (unsigned)time.wMinute, (unsigned)time.wSecond);
    if (!backfill) packet->chat.record.receivedAt = timestamp;
    uiPost(WM_CHAT, packet);
}

bool randomBytes(BYTE *dst,DWORD count) {
    HCRYPTPROV h=0;
    if (!CryptAcquireContextW(&h,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)) return false;
    BOOL ok=CryptGenRandom(h,count,dst); CryptReleaseContext(h,0);
    return ok!=0;
}
std::string base64(const BYTE *p,size_t n) {
    static const char chars[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i=0;i<n;i+=3) {
        unsigned a=p[i], b=i+1<n?p[i+1]:0, c=i+2<n?p[i+2]:0;
        out+=chars[a>>2]; out+=chars[((a&3)<<4)|(b>>4)];
        out+=i+1<n ? chars[((b&15)<<2)|(c>>6)] : '=';
        out+=i+2<n ? chars[c&63] : '=';
    }
    return out;
}

std::wstring exeFile(const wchar_t *name) {
    wchar_t file[MAX_PATH]={0};GetModuleFileNameW(NULL,file,MAX_PATH);
    std::wstring p(file); size_t slash=p.find_last_of(L"\\/");
    return (slash==std::wstring::npos?L"":p.substr(0,slash+1))+name;
}

std::string utf8FromWide(const std::wstring &w) {
    if(w.empty())return "";
    int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w.c_str(),(int)w.size(),NULL,0,NULL,NULL);
    if(n<=0)return "";std::string s(n,'\0');
    if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w.c_str(),(int)w.size(),&s[0],n,NULL,NULL))return "";
    return s;
}
