#include "kickchat_websocket.hpp"
#include "kickchat_websocket_handshake.hpp"
#include "kickchat_platform.hpp"

static std::string websocketAccept(const std::string &key) {
    std::string data=key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    HCRYPTPROV h=0; HCRYPTHASH hash=0; BYTE digest[20]; DWORD n=sizeof(digest);
    if (!CryptAcquireContextW(&h,NULL,NULL,PROV_RSA_FULL,CRYPT_VERIFYCONTEXT)) return "";
    bool ok=CryptCreateHash(h,CALG_SHA1,0,0,&hash)!=0;
    if (ok) ok=CryptHashData(hash,(BYTE*)data.data(),(DWORD)data.size(),0)!=0;
    if (ok) ok=CryptGetHashParam(hash,HP_HASHVAL,digest,&n,0)!=0;
    if (hash) CryptDestroyHash(hash); CryptReleaseContext(h,0);
    return ok?base64(digest,n):"";
}

static std::string stringifyCloseCode(unsigned code) {char buf[32];sprintf(buf,"%u",code);return buf;}


WebSocket::WebSocket(TlsTransport &transport) : tls(transport) {}

bool WebSocket::open(const std::string &host, const std::wstring &wideHost, const std::string &appKey) {
    if (!tls.connectTls(host.c_str(),wideHost.c_str())) return false;
    BYTE nonce[16]; if (!randomBytes(nonce,sizeof(nonce))) {
        recordCode(L"Aleatoriedad WebSocket",GetLastError());return false;
    }
    std::string key=base64(nonce,sizeof(nonce));
    const std::string request=websocketHandshake::request(host,appKey,key);
    const std::string expected=websocketAccept(key);
    if (request.empty() || expected.empty() || !tls.write(request)) return false;
    std::string response; DWORD start=GetTickCount();
    while (running() && (DWORD)(GetTickCount()-start)<12000) {
        std::string chunk;int status=tls.readPlain(chunk);
        if (status<0) return false;
        response+=chunk;
        size_t headerBytes=0;
        const websocketHandshake::Result result=
            websocketHandshake::validate(response,expected,headerBytes);
        if (result==websocketHandshake::Accepted)
            return frames.append(response.substr(headerBytes));
        if (result==websocketHandshake::NeedMore) continue;
        recordError(L"WebSocket",L"Respuesta HTTP Upgrade invalida ("+
                    toWide(std::to_string(static_cast<int>(result)))+L")");
        return false;
    }
    if (running()) recordError(L"WebSocket",L"No llego respuesta HTTP 101 (12 s)");
    return false;
}

bool WebSocket::sendFrame(int opcode, const std::string &payload) {
    if (payload.size() > 1024 * 1024) return false;
    BYTE mask[4];
    if (!randomBytes(mask, sizeof(mask))) {
        recordCode(L"Aleatoriedad WebSocket", GetLastError());
        return false;
    }
    std::string frame;
    return websocketFrame::encode(static_cast<unsigned>(opcode), payload, mask, frame) &&
           tls.write(frame);
}

bool WebSocket::sendText(const std::string &text) { return sendFrame(1, text); }

int WebSocket::nextText(std::string &out) {
    while (running()) {
        std::string payload;
        unsigned closeCode = 0;
        const websocketFrame::Result result = frames.next(payload, closeCode);
        if (result == websocketFrame::Text) { out.swap(payload); return 1; }
        if (result == websocketFrame::Ping) {
            if (!sendFrame(10, payload)) return -1;
            continue;
        }
        if (result == websocketFrame::Pong) continue;
        if (result == websocketFrame::Closed) {
            std::wstring reason = L"Servidor cerro WebSocket";
            if (closeCode)
                reason += L" (codigo " + toWide(stringifyCloseCode(closeCode)) + L")";
            recordError(L"WebSocket", reason);
            return -1;
        }
        if (result == websocketFrame::Invalid) return -1;
        std::string data;
        const int status = tls.readPlain(data);
        if (status <= 0) return status;
        if (!frames.append(data)) return -1;
    }
    return -1;
}
