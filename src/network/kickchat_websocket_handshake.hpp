#ifndef KICKCHAT_WEBSOCKET_HANDSHAKE_HPP
#define KICKCHAT_WEBSOCKET_HANDSHAKE_HPP

#include <cstddef>
#include <string>

namespace websocketHandshake {

enum Result {
    NeedMore,
    Accepted,
    InvalidStatus,
    InvalidHeaders,
    MissingUpgrade,
    MissingConnection,
    MissingAccept,
    InvalidAccept,
    TooLarge
};

std::string request(const std::string &host, const std::string &appKey,
                    const std::string &clientKey);
Result validate(const std::string &response, const std::string &expectedAccept,
                size_t &headerBytes);

} // namespace websocketHandshake

#endif
