#include "kickchat_tls_curl.hpp"
#include "kickchat_curl_backend.hpp"
#include "kickchat_platform.hpp"
#include <cstring>
#include <cwchar>

namespace {

bool validTlsHost(const char *host, const wchar_t *hostname) {
    if (!host || !hostname) return false;
    const size_t length = std::strlen(host);
    if (length == 0 || length > 253 || std::wcslen(hostname) != length) return false;
    for (size_t i = 0; i < length; ++i) {
        const char c = host[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '.') ||
            hostname[i] != static_cast<wchar_t>(c)) return false;
    }
    return hostname[length] == L'\0';
}

} // namespace

CurlTlsTransport::CurlTlsTransport() : handle(NULL), connected(false) {}

CurlTlsTransport::~CurlTlsTransport() {
    if (handle) curl_easy_cleanup(handle);
}

bool CurlTlsTransport::connectTls(const char *host, const wchar_t *hostname) {
    if (handle || !validTlsHost(host, hostname) || !curlBackend::ready()) return false;
    handle = curl_easy_init();
    if (!handle) return false;
    const std::string url = std::string("https://") + host + "/";
    const bool configured = curlBackend::configureTls(handle) &&
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str()) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 1L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 5000L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 15000L) == CURLE_OK &&
        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L) == CURLE_OK;
    const CURLcode code = configured ? curl_easy_perform(handle) : CURLE_FAILED_INIT;
    if (code == CURLE_OK) { connected = true; return true; }
    recordError(L"WebSocket curl TLS", configured ? toWide(curl_easy_strerror(code)) :
                L"No se pudo configurar TLS o cargar los certificados CA");
    curl_easy_cleanup(handle);
    handle = NULL;
    return false;
}

bool CurlTlsTransport::write(const std::string &data) {
    if (!connected) return false;
    size_t position = 0;
    DWORD lastProgress = GetTickCount();
    while (position < data.size() && running()) {
        size_t sent = 0;
        const CURLcode code = curl_easy_send(handle, data.data() + position,
                                            data.size() - position, &sent);
        if (code == CURLE_OK && sent) {
            position += sent;
            lastProgress = GetTickCount();
            continue;
        }
        if (code != CURLE_AGAIN || (DWORD)(GetTickCount() - lastProgress) >= 5000) {
            recordError(L"WebSocket curl envio", code == CURLE_OK ?
                        L"No se enviaron datos" : toWide(curl_easy_strerror(code)));
            return false;
        }
        Sleep(20);
    }
    return position == data.size();
}

int CurlTlsTransport::readPlain(std::string &output) {
    if (!connected || !running()) return -1;
    char buffer[8192];
    size_t received = 0;
    const CURLcode code = curl_easy_recv(handle, buffer, sizeof(buffer), &received);
    if (code == CURLE_OK && received) {
        output.append(buffer, received);
        return 1;
    }
    if (code != CURLE_AGAIN) {
        recordError(L"WebSocket curl lectura", code == CURLE_OK ?
                    L"El servidor cerro la conexion" : toWide(curl_easy_strerror(code)));
        return -1;
    }
    curl_socket_t socket = CURL_SOCKET_BAD;
    if (curl_easy_getinfo(handle, CURLINFO_ACTIVESOCKET, &socket) != CURLE_OK ||
        socket == CURL_SOCKET_BAD) return -1;
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socket, &readSet);
    timeval timeout = {1, 0};
    const int result = select(0, &readSet, NULL, NULL, &timeout);
    if (result < 0) {
        recordCode(L"WebSocket curl select", static_cast<DWORD>(WSAGetLastError()));
        return -1;
    }
    return 0;
}
