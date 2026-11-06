#include "kickchat_api_http.hpp"
#include "kickchat_json.hpp"
#include "kickchat_platform.hpp"
#ifndef KICKCHAT_USE_CURL
#include <winhttp.h>

struct HttpConnectionPool {
    HINTERNET session;
    std::map<std::wstring, HINTERNET> connections;

    HttpConnectionPool() : session(NULL) {}
    ~HttpConnectionPool() {
        for (std::map<std::wstring, HINTERNET>::iterator it=connections.begin();
             it!=connections.end(); ++it) WinHttpCloseHandle(it->second);
        if (session) WinHttpCloseHandle(session);
    }

    HINTERNET connect(const wchar_t *host, std::string &error) {
        if (!session) {
            session=WinHttpOpen(L"KickChatWin7/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session) { error="WinHttpOpen"; return NULL; }
            DWORD protocols=0x00000800; // TLS 1.2 on Windows 7 SP1
            WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
            WinHttpSetTimeouts(session, 5000, 5000, 12000, 12000);
        }
        const std::wstring key(host);
        std::map<std::wstring, HINTERNET>::iterator it=connections.find(key);
        if (it!=connections.end()) return it->second;
        HINTERNET connection=WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connection) { error="WinHttpConnect"; return NULL; }
        connections[key]=connection;
        return connection;
    }
};

HttpResult kickHttp(const wchar_t *host, const wchar_t *path, const wchar_t *method,
                    const std::string &body, const std::string &bearer, bool form) {
    HttpResult out;
    // OAuth and API requests are serialized by g_taskBusy.
    static HttpConnectionPool pool;
    HINTERNET connection=pool.connect(host,out.error);
    if(!connection) return out;
    HINTERNET req=WinHttpOpenRequest(connection,method,path,NULL,WINHTTP_NO_REFERER,
                                    WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);
    if(!req){out.error="WinHttpOpenRequest";return out;}
    std::wstring headers=L"Accept: application/json\r\n";
    if(!body.empty())headers+=form?L"Content-Type: application/x-www-form-urlencoded\r\n"
                                  :L"Content-Type: application/json; charset=utf-8\r\n";
    if(!bearer.empty())headers+=L"Authorization: Bearer "+toWide(bearer)+L"\r\n";
    BOOL ok=WinHttpSendRequest(req,headers.c_str(),(DWORD)-1,
                               body.empty()?WINHTTP_NO_REQUEST_DATA:(LPVOID)body.data(),
                               (DWORD)body.size(),(DWORD)body.size(),0);
    if(ok)ok=WinHttpReceiveResponse(req,NULL);
    if(ok){
        DWORD length=sizeof(out.status);
        WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,&out.status,&length,WINHTTP_NO_HEADER_INDEX);
        char buf[4096];DWORD n=0;
        while(WinHttpReadData(req,buf,sizeof(buf),&n)&&n){
            if(out.body.size()+n>131072){out.error="Respuesta HTTP demasiado larga";break;}
            out.body.append(buf,n);
        }
    }else{
        char b[40];sprintf(b,"WinHTTP error %lu",(unsigned long)GetLastError());out.error=b;
    }
    WinHttpCloseHandle(req);
    return out;
}
#endif
std::wstring apiError(const HttpResult &r) {
    if(!r.error.empty()) {
#ifdef KICKCHAT_USE_CURL
        return toWide(r.error)+L" (comprobar OpenSSL y certificados CA)";
#else
        return toWide(r.error)+L" (comprobar TLS 1.2 de Windows 7)";
#endif
    }
    Json j;std::string detail;
    if(parseJson(r.body,j)) {
        detail=j.get("message").str();if(detail.empty())detail=j.get("error_description").str();
        if(detail.empty())detail=j.get("error").str();
    }
    // Do not log raw HTTP bodies; OAuth responses may contain credentials.
    if(detail.size()>180)detail.resize(180);
    wchar_t status[30];swprintf(status,30,L"HTTP %lu",(unsigned long)r.status);
    return std::wstring(status)+(detail.empty()?L"":L": "+toWide(detail));
}
