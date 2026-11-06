#include "kickchat_websocket_handshake.hpp"
#include <cctype>

namespace websocketHandshake {

static const size_t MaximumHeaders = 32768;

static bool safe(const std::string &value, const std::string &characters) {
    if (value.empty()) return false;
    for (size_t i = 0; i < value.size(); ++i) {
        if (characters.find(value[i]) == std::string::npos) return false;
    }
    return true;
}

std::string request(const std::string &host, const std::string &appKey,
                    const std::string &clientKey) {
    const std::string hostCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-";
    const std::string keyCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const std::string base64Characters = keyCharacters + "+/=";
    if (host.size() > 253 || appKey.size() > 64 || clientKey.size() != 24 ||
        !safe(host, hostCharacters) || !safe(appKey, keyCharacters) ||
        !safe(clientKey, base64Characters)) return "";
    const std::string path = "/app/" + appKey +
        "?protocol=7&client=js&version=8.5.0&flash=false";
    return "GET " + path + " HTTP/1.1\r\nHost: " + host +
        "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: " + clientKey +
        "\r\nSec-WebSocket-Version: 13\r\nOrigin: https://kick.com\r\n"
        "User-Agent: Mozilla/5.0\r\n\r\n";
}

static std::string lower(const std::string &value) {
    std::string result = value;
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    return result;
}

static std::string trim(const std::string &value) {
    const size_t begin = value.find_first_not_of(" \t");
    if (begin == std::string::npos) return "";
    const size_t end = value.find_last_not_of(" \t");
    return value.substr(begin, end - begin + 1);
}

static bool containsToken(const std::string &value, const std::string &token) {
    size_t start = 0;
    while (start < value.size()) {
        const size_t end = value.find(',', start);
        if (lower(trim(value.substr(start, end == std::string::npos ? end : end - start))) == token)
            return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

Result validate(const std::string &response, const std::string &expectedAccept,
                size_t &headerBytes) {
    headerBytes = 0;
    const size_t end = response.find("\r\n\r\n");
    if (end == std::string::npos) return response.size() >= MaximumHeaders ? TooLarge : NeedMore;
    headerBytes = end + 4;
    if (headerBytes > MaximumHeaders) return TooLarge;
    const size_t firstEnd = response.find("\r\n");
    const std::string status = response.substr(0, firstEnd);
    if (status.compare(0, 12, "HTTP/1.1 101") != 0 ||
        (status.size() > 12 && status[12] != ' ')) return InvalidStatus;

    bool upgrade = false, connection = false, accept = false;
    size_t start = firstEnd + 2;
    while (start < end) {
        const size_t lineEnd = response.find("\r\n", start);
        if (lineEnd == std::string::npos || lineEnd > end) return InvalidHeaders;
        const std::string line = response.substr(start, lineEnd - start);
        const size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0 || line[0] == ' ' || line[0] == '\t')
            return InvalidHeaders;
        for (size_t i = 0; i < line.size(); ++i)
            if (static_cast<unsigned char>(line[i]) < 32 && line[i] != '\t')
                return InvalidHeaders;
        const std::string name = lower(line.substr(0, colon));
        const std::string value = trim(line.substr(colon + 1));
        if (name == "upgrade") upgrade = upgrade || containsToken(value, "websocket");
        else if (name == "connection") connection = connection || containsToken(value, "upgrade");
        else if (name == "sec-websocket-accept") {
            if (accept) return InvalidHeaders;
            accept = true;
            if (expectedAccept.empty() || value != expectedAccept) return InvalidAccept;
        }
        start = lineEnd + 2;
    }
    if (!upgrade) return MissingUpgrade;
    if (!connection) return MissingConnection;
    if (!accept) return MissingAccept;
    return Accepted;
}

} // namespace websocketHandshake
