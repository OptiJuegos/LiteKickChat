#include "kickchat_oauth_callback.hpp"
#include "kickchat_strings.hpp"
#include "kickchat_oauth.hpp"

static std::string urlDecode(const std::string &s) {
    std::string result;for(size_t i=0;i<s.size();i++){
        if(s[i]=='%'&&i+2<s.size()){
            char a=s[i+1],b=s[i+2];
            int x=(a>='0'&&a<='9')?a-'0':(a>='A'&&a<='F')?a-'A'+10:(a>='a'&&a<='f')?a-'a'+10:-1;
            int y=(b>='0'&&b<='9')?b-'0':(b>='A'&&b<='F')?b-'A'+10:(b>='a'&&b<='f')?b-'a'+10:-1;
            if(x>=0&&y>=0){result+=(char)(x*16+y);i+=2;continue;}
        }
        result+=(s[i]=='+')?' ':s[i];
    }return result;
}

static std::string queryParameter(const std::string &query,const std::string &key) {
    size_t pos=0;while(pos<query.size()){
        size_t end=query.find('&',pos);if(end==std::string::npos)end=query.size();
        size_t equals=query.find('=',pos);
        if(equals!=std::string::npos&&equals<end&&query.substr(pos,equals-pos)==key)
            return urlDecode(query.substr(equals+1,end-equals-1));
        pos=end+1;
    }return "";
}

bool parseOAuthCallbackRequest(const std::string &request, const std::string &expectedState,
                               std::string &code, std::string &error) {
    size_t first=request.find(' '),second=first==std::string::npos?std::string::npos:request.find(' ',first+1);
    std::string target=second==std::string::npos?"":request.substr(first+1,second-first-1);
    size_t qm=target.find('?');std::string path=target.substr(0,qm);
    std::string query=qm==std::string::npos?"":target.substr(qm+1);
    std::string state=queryParameter(query,"state");
    bool valid=request.compare(0,4,"GET ")==0&&path=="/callback"&&state==expectedState;
    code=valid?queryParameter(query,"code"):"";
    error=valid?queryParameter(query,"error"):"";
    return valid;
}

bool receiveOAuthCallback(SOCKET listener, const std::string &expectedState,
                          std::string &code, std::wstring &reason) {
    DWORD start=GetTickCount();
    while(running()&&(DWORD)(GetTickCount()-start)<120000) {
        if(InterlockedCompareExchange(&g_oauthCancelRequested,0,0)!=0){
        reason=kickchatText::get(kickchatText::AuthCancelled);return false;
        }
        fd_set readSet;FD_ZERO(&readSet);FD_SET(listener,&readSet);
        timeval timeout;timeout.tv_sec=1;timeout.tv_usec=0;
        int ready=select(0,&readSet,NULL,NULL,&timeout);if(ready<=0)continue;
        SOCKET sock=accept(listener,NULL,NULL);if(sock==INVALID_SOCKET)continue;
        DWORD limit=5000;setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&limit,sizeof(limit));
        char buf[4096];int n=recv(sock,buf,sizeof(buf)-1,0);
        if(n<=0){closesocket(sock);continue;}buf[n]=0;
        std::string request(buf,n),authCode,err;
        const bool valid=parseOAuthCallbackRequest(request,expectedState,authCode,err);
        const char*success="<html><body><h3>KickChat autorizado. Ya podes cerrar esta pestana.</h3></body></html>";
        const char*failure="<html><body><h3>La autorizacion no se completo. Volve al programa.</h3></body></html>";
        const char*html=(valid&&!authCode.empty())?success:failure;
        char response[512];int count=sprintf(response,"HTTP/1.1 %s\r\nContent-Type: text/html; charset=UTF-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: %u\r\n\r\n",(valid&&!authCode.empty())?"200 OK":"400 Bad Request",(unsigned)strlen(html));
        send(sock,response,count,0);send(sock,html,(int)strlen(html),0);closesocket(sock);
        if(!valid)continue;
    if(!err.empty()){reason=kickchatText::get(kickchatText::OAuthCallbackRejected)+toWide(err);return false;}
        if(!authCode.empty()&&authCode.size()<2048){code=authCode;return true;}
    }
    reason=InterlockedCompareExchange(&g_oauthCancelRequested,0,0)!=0
            ?kickchatText::get(kickchatText::AuthCancelled)
            :kickchatText::get(kickchatText::OAuthCallbackTimeout);
    return false;
}
