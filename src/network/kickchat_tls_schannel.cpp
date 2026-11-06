#include "kickchat_tls_schannel.hpp"

static bool sendAll(SOCKET sock,const char *data,size_t n) {
    while (n && running()) {
        int chunk=(int)std::min(n,(size_t)32768);
        int sent=send(sock,data,chunk,0);
        if (sent<=0) {
            recordCode(L"TCP escritura",(DWORD)WSAGetLastError());
            return false;
        }
        data+=sent; n-=sent;
    }
    return n==0;
}


int Tls::receiveRaw(std::vector<char> &dst) {
    char temp[8192]; int n=recv(sock,temp,sizeof(temp),0);
    if (n>0) {dst.insert(dst.end(),temp,temp+n);return OK;}
    if (n<0) {
        int code=WSAGetLastError();
        if (code==WSAETIMEDOUT || code==WSAEWOULDBLOCK) return TIMEOUT;
        recordCode(L"TCP lectura",(DWORD)code);
    } else recordError(L"TCP",L"Servidor cerro la conexion");
    return FAIL;
}

Tls::Tls():sock(INVALID_SOCKET),hasCred(false),hasCtx(false) {ZeroMemory(&cred,sizeof(cred));ZeroMemory(&ctx,sizeof(ctx));ZeroMemory(&sizes,sizeof(sizes));}

Tls::~Tls() {closeTls();}

void Tls::closeTls() {
    if (hasCtx) {DeleteSecurityContext(&ctx); hasCtx=false;}
    if (hasCred){FreeCredentialsHandle(&cred); hasCred=false;}
    if (sock!=INVALID_SOCKET){closesocket(sock);sock=INVALID_SOCKET;}
}

bool Tls::connectTls(const char *host,const wchar_t *hostname) {
    addrinfo hints={0}; hints.ai_socktype=SOCK_STREAM; hints.ai_family=AF_UNSPEC;
    addrinfo *addrs=NULL;
    int dns=getaddrinfo(host,"443",&hints,&addrs);
    if (dns!=0) {recordCode(L"DNS",(DWORD)dns);return false;}
    DWORD lastSock=WSAETIMEDOUT;
    for (addrinfo *p=addrs;p && running();p=p->ai_next) {
        SOCKET s=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
        if (s==INVALID_SOCKET) continue;
        u_long nonblock=1; ioctlsocket(s,FIONBIO,&nonblock);
        int result=::connect(s,p->ai_addr,(int)p->ai_addrlen);
        if (result==SOCKET_ERROR && WSAGetLastError()==WSAEWOULDBLOCK) {
            fd_set writeSet, exceptSet; FD_ZERO(&writeSet);FD_ZERO(&exceptSet);FD_SET(s,&writeSet);FD_SET(s,&exceptSet);
            timeval tv;tv.tv_sec=5;tv.tv_usec=0;
            result=select(0,NULL,&writeSet,&exceptSet,&tv);
            if (result>0 && FD_ISSET(s,&writeSet)) {
                int err=1;int sz=sizeof(err);
                getsockopt(s,SOL_SOCKET,SO_ERROR,(char*)&err,&sz);
                result=err==0?0:SOCKET_ERROR;
                if (err!=0) lastSock=(DWORD)err;
            } else {result=SOCKET_ERROR;lastSock=WSAETIMEDOUT;}
        }
        nonblock=0;ioctlsocket(s,FIONBIO,&nonblock);
        if (result==0) {sock=s;break;}
        if (WSAGetLastError()!=0 && lastSock==WSAETIMEDOUT) lastSock=(DWORD)WSAGetLastError();
        closesocket(s);
    }
    freeaddrinfo(addrs);
    if (sock==INVALID_SOCKET) {recordCode(L"TCP connect",lastSock);return false;}
    DWORD timeout=1000;
    setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
    timeout=5000;setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout,sizeof(timeout));
    SCHANNEL_CRED opts; ZeroMemory(&opts,sizeof(opts));
    opts.dwVersion=SCHANNEL_CRED_VERSION;
    opts.grbitEnabledProtocols=SP_PROT_TLS1_2_CLIENT;
    opts.dwFlags=SCH_CRED_AUTO_CRED_VALIDATION;
    TimeStamp expiry;
    SECURITY_STATUS status=AcquireCredentialsHandleW(NULL,(LPWSTR)UNISP_NAME_W,SECPKG_CRED_OUTBOUND,NULL,&opts,NULL,NULL,&cred,&expiry);
    if (status!=SEC_E_OK) {recordCode(L"SChannel credenciales TLS 1.2",(DWORD)status);return false;}
    hasCred=true;
    const DWORD flags=ISC_REQ_SEQUENCE_DETECT|ISC_REQ_REPLAY_DETECT|ISC_REQ_CONFIDENTIALITY|ISC_REQ_EXTENDED_ERROR|ISC_REQ_ALLOCATE_MEMORY|ISC_REQ_STREAM;
    std::vector<char> input;
    bool first=true;
    DWORD last=GetTickCount();
    while (running() && (DWORD)(GetTickCount()-last)<15000) {
        if (!first && input.empty()) {
            int got=receiveRaw(input);
            if (got==FAIL) return false;
            if (got==TIMEOUT) continue;
        }
        SecBuffer outbuf={0,SECBUFFER_TOKEN,NULL};
        SecBufferDesc outdesc={SECBUFFER_VERSION,1,&outbuf};
        SecBuffer inbuf[2]={{(ULONG)input.size(),SECBUFFER_TOKEN,input.empty()?NULL:&input[0]},{0,SECBUFFER_EMPTY,NULL}};
        SecBufferDesc indesc={SECBUFFER_VERSION,2,inbuf};
        DWORD attr=0;
        status=InitializeSecurityContextW(&cred,first?NULL:&ctx,(SEC_WCHAR*)hostname,flags,0,SECURITY_NATIVE_DREP,
            first?NULL:&indesc,0,&ctx,&outdesc,&attr,&expiry);
        if (status==SEC_E_INCOMPLETE_MESSAGE) {
            int got=receiveRaw(input);
            if (got==FAIL) return false;
            continue;
        }
        first=false; hasCtx=true;
        if (outbuf.pvBuffer && outbuf.cbBuffer) {
            bool sent=sendAll(sock,(char*)outbuf.pvBuffer,outbuf.cbBuffer);
            FreeContextBuffer(outbuf.pvBuffer);
            if (!sent) return false;
        } else if (outbuf.pvBuffer) FreeContextBuffer(outbuf.pvBuffer);
        if (status!=SEC_E_OK && status!=SEC_I_CONTINUE_NEEDED) {
            recordCode(L"SChannel handshake TLS 1.2",(DWORD)status);
            return false;
        }
        if (inbuf[1].BufferType==SECBUFFER_EXTRA && inbuf[1].cbBuffer<=input.size()) {
            std::vector<char> extra(input.end()-inbuf[1].cbBuffer,input.end()); input.swap(extra);
        } else input.clear();
        if (status==SEC_E_OK) {
            encrypted.swap(input);
            SECURITY_STATUS sizesStatus=QueryContextAttributes(&ctx,SECPKG_ATTR_STREAM_SIZES,&sizes);
            if (sizesStatus!=SEC_E_OK) recordCode(L"SChannel stream sizes",(DWORD)sizesStatus);
            return sizesStatus==SEC_E_OK;
        }
    }
    if (running()) recordError(L"SChannel TLS 1.2",L"Tiempo agotado al negociar TLS (15 s)");
    return false;
}

bool Tls::write(const std::string &data) {
    if (!hasCtx || sizes.cbMaximumMessage==0) return false;
    size_t off=0;
    while (off<data.size() && running()) {
        size_t count=std::min((size_t)sizes.cbMaximumMessage,data.size()-off);
        std::vector<char> buf((size_t)sizes.cbHeader+count+sizes.cbTrailer);
        memcpy(&buf[sizes.cbHeader],data.data()+off,count);
        SecBuffer b[4]={{sizes.cbHeader,SECBUFFER_STREAM_HEADER,&buf[0]},
            {(ULONG)count,SECBUFFER_DATA,&buf[sizes.cbHeader]},
            {sizes.cbTrailer,SECBUFFER_STREAM_TRAILER,&buf[sizes.cbHeader+count]},
            {0,SECBUFFER_EMPTY,NULL}};
        SecBufferDesc desc={SECBUFFER_VERSION,4,b};
        SECURITY_STATUS security=EncryptMessage(&ctx,0,&desc,0);
        if (security!=SEC_E_OK) {recordCode(L"SChannel cifrado",(DWORD)security);return false;}
        size_t length=b[0].cbBuffer+b[1].cbBuffer+b[2].cbBuffer;
        if (!sendAll(sock,&buf[0],length)) return false;
        off+=count;
    }
    return off==data.size();
}

int Tls::readPlain(std::string &out) {
    for (int k=0;k<12 && running();++k) {
        if (encrypted.empty()) {
            int got=receiveRaw(encrypted);
            if (got!=OK) return got;
        }
        SecBuffer b[4]={{(ULONG)encrypted.size(),SECBUFFER_DATA,&encrypted[0]},
            {0,SECBUFFER_EMPTY,NULL},{0,SECBUFFER_EMPTY,NULL},{0,SECBUFFER_EMPTY,NULL}};
        SecBufferDesc desc={SECBUFFER_VERSION,4,b};
        SECURITY_STATUS status=DecryptMessage(&ctx,&desc,0,NULL);
        if (status==SEC_E_INCOMPLETE_MESSAGE) {
            if (encrypted.size()>2*1024*1024) return FAIL;
            int got=receiveRaw(encrypted);
            if (got!=OK) return got;
            continue;
        }
        if (status!=SEC_E_OK) {recordCode(L"SChannel descifrado",(DWORD)status);return FAIL;}
        for (int j=0;j<4;j++) if (b[j].BufferType==SECBUFFER_DATA && b[j].cbBuffer && b[j].pvBuffer)
            out.append((char*)b[j].pvBuffer,b[j].cbBuffer);
        size_t extra=0;
        for (int j=0;j<4;j++) if (b[j].BufferType==SECBUFFER_EXTRA) extra=b[j].cbBuffer;
        if (extra && extra<=encrypted.size()) {
            std::vector<char> rest(encrypted.end()-extra,encrypted.end());encrypted.swap(rest);
        } else encrypted.clear();
        if (!out.empty()) return OK;
    }
    return out.empty()?TIMEOUT:OK;
}

