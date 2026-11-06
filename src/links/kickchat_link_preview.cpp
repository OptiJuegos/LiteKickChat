#include "kickchat_link_preview.hpp"
#include <cctype>

static bool validDomain(const std::string &host) {
    if (host.empty() || host.size() > 253) return false;
    const size_t dot = host.rfind('.');
    if (dot == std::string::npos || dot + 3 > host.size()) return false;
    for (size_t i = dot + 1; i < host.size(); ++i)
        if (!std::isalpha(static_cast<unsigned char>(host[i]))) return false;
    size_t from = 0;
    while (from < host.size()) {
        size_t end = host.find('.', from);
        if (end == std::string::npos) end = host.size();
        if (end == from || end - from > 63 ||
            !std::isalnum(static_cast<unsigned char>(host[from])) ||
            !std::isalnum(static_cast<unsigned char>(host[end - 1]))) return false;
        for (size_t i = from; i < end; ++i) {
            const unsigned char c = static_cast<unsigned char>(host[i]);
            if (!std::isalnum(c) && c != '-') return false;
        }
        from = end + 1;
    }
    return true;
}

static bool parseLink(const std::string &candidate, LinkPreview &preview) {
    const bool secure = candidate.compare(0, 8, "https://") == 0;
    const bool plain = candidate.compare(0, 7, "http://") == 0;
    const size_t hostStart = secure ? 8 : plain ? 7 : 0;
    const size_t path = candidate.find_first_of("/?#", hostStart);
    const std::string authority = candidate.substr(hostStart, path == std::string::npos ?
                                                   std::string::npos : path - hostStart);
    const size_t colon = authority.find(':');
    const std::string host = authority.substr(0, colon);
    if (!validDomain(host)) return false;
    if (colon != std::string::npos) {
        const std::string port = authority.substr(colon + 1);
        if (port.empty() || port.size() > 5) return false;
        unsigned number = 0;
        for (size_t i = 0; i < port.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(port[i]))) return false;
            number = number * 10 + (port[i] - '0');
        }
        if (number == 0 || number > 65535) return false;
    }
    preview.url = (secure || plain ? "" : "https://") + candidate;
    preview.host = host;
    return preview.url.size() <= 2048;
}

bool findNextLink(const std::string &message, size_t from, LinkSpan &match) {
    while (from < message.size()) {
        while (from < message.size() && (std::isspace(static_cast<unsigned char>(message[from])) ||
               message[from] == '(' || message[from] == '[' || message[from] == '<' ||
               message[from] == '"' || message[from] == '\'' || message[from] == ':' ||
               message[from] == ',' || message[from] == '>' ||
               message[from] == ')' || message[from] == ']')) ++from;
        if (from == message.size()) return false;
        size_t end = from;
        while (end < message.size() && !std::isspace(static_cast<unsigned char>(message[end])) &&
               message[end] != '<' && message[end] != '>' && message[end] != '"' &&
               message[end] != '\'') ++end;
        size_t trimmed = end;
        while (trimmed > from && (message[trimmed - 1] == '.' || message[trimmed - 1] == ',' ||
               message[trimmed - 1] == '!' || message[trimmed - 1] == ')' ||
               message[trimmed - 1] == ']' || message[trimmed - 1] == '}' ||
               message[trimmed - 1] == ';' || message[trimmed - 1] == ':' ||
               message[trimmed - 1] == '?')) --trimmed;
        if (trimmed > from && trimmed - from <= 2048) {
            LinkPreview preview;
            if (parseLink(message.substr(from, trimmed - from), preview)) {
                match.start = from;
                match.length = trimmed - from;
                match.preview = preview;
                return true;
            }
        }
        from = end;
    }
    return false;
}

bool findLinkPreview(const std::string &message, LinkPreview &preview) {
    LinkSpan match;
    if (!findNextLink(message, 0, match)) return false;
    preview = match.preview;
    return true;
}
